;==============================================================================
; プロジェクト : P629_Kodo_Winker
; ファイル     : P629_Kodo_Winker_Ver0.06.asm
; 対象CPU      : PIC12F629
; アセンブラ   : MPASM
;
; Ver.0.06
;
; 【変更内容】
;   ・PIC16F84A版（Ver.0.76）の全機能をPIC12F629へ新規移植
;   ・内部4MHz発振（_INTRC_OSC_NOCLKOUT）を採用し、外付け発振部品を不要化
;   ・工場出荷時の発振校正値（0x3FF）をOSCCALへ自動ロードする初期化処理を実装
;   ・コンパレータを無効化（CMCON = 0x07）し、全ピンをデジタルI/Oとして設定
;   ・GP0(PLUS)、GP1(MINUS)、GP2(SELECT)に内部プルアップ（WPU）を設定
;   ・GP3(WINKER_IN)をアクティブHigh入力（＋入力で点滅ON、0Vで消灯待機）として実装
;   ・GP4をLED_PWM出力（アクティブHigh）に設定
;   ・デバッグ用の測定トグル出力を廃止し、GP5をLOW出力固定（予備）に設定
;   ・P84 Ver.0.77のフェード時間配分（上昇SLOPE+2、下降16-SLOPE）を移植
;   ・Timer0初期値を234へ変更（4MHz環境で約33us/割り込みを狙う暫定設定）
;   ・フレーム時間に合わせて操作/フィードバック時間定数を再設定
;   ・完全消灯時間を全周期の25%へ設定
;   ・周期テーブルを再換算し、全周期の設定範囲を約500～1500msに維持
;   ・消灯フェーズのカウントをUPDATE_FADE呼び出し単位からフレーム単位に変更（実機安定化）
;
; 【I/O設定】
;   GP0 (Pin 7) : PLUS（設定値UP / アクティブLow / 内部プルアップ有効）
;   GP1 (Pin 6) : MINUS（設定値DOWN / アクティブLow / 内部プルアップ有効）
;   GP2 (Pin 5) : SELECT（短押し：周期/傾斜切替、3秒長押し：EEPROM保存 / アクティブLow / 内部プルアップ有効）
;   GP3 (Pin 4) : WINKER_IN（ウインカー入力 / アクティブHigh：HIGHで点滅ON、LOWで消灯待機）
;   GP4 (Pin 3) : LED_PWM出力（アクティブHigh）
;   GP5 (Pin 2) : 予備（出力LOW固定）
;
; 【発振】
;   内部RC発振 4MHz（_INTRC_OSC_NOCLKOUT）
;
; 【Timer0】
;   内部クロック
;   プリスケーラなし（WDT側へ割り当て）
;   初期値234（約33us/割り込みを狙う設定、PWMフレーム約2.112msの見込み（64×約33us））
;   ※実際の割り込み周期はISR遅延を含むため、実機測定で確定すること
;
; 【変更履歴】
;   Ver.0.01 : PIC12F629向け新規作成（P84 Ver.0.76ベース移植）
;   Ver.0.02 : P84 Ver.0.77のフェード配分/周期テーブルを反映、Timer0と時間定数を再調整
;   Ver.0.03 : 完全消灯を25%へ拡大し、周期テーブルを再換算
;   Ver.0.04 : 消灯を10%に戻し、PERIOD_TABLEはVer0.03を維持（検証用）
;   Ver.0.05 : PERIOD_TABLEをVer0.02に戻し、消灯25%を維持（検証用）
;   Ver.0.06 : 新テーブル＋25%消灯。消灯カウントをフレーム単位に変更して安定化
;
; 【検証状態】
;   ・MPASMによるアセンブル未実施
;   ・Timer0周期/フレーム周期は実機オシロスコープで要測定
;   ・GP3は内部プルアップなし。外部プルダウン抵抗を必ず実装
;   ・Ver.0.06は検証用ブランチ。実機動作確認済みではない
;==============================================================================

        LIST    P=12F629
        #include <P12F629.INC>

        __CONFIG _CP_OFF & _CPD_OFF & _BODEN_ON & _MCLRE_OFF & _WDT_OFF & _PWRTE_ON & _INTRC_OSC_NOCLKOUT

;------------------------------------------------------------------------------
; 定数定義
;------------------------------------------------------------------------------

EEPROM_MAGIC    EQU     0xA5            ; EEPROMデータ有効確認用マジックナンバー
DEBOUNCE_FRAMES EQU     .120             ; チャタリング防止期間（約250ms）
HOLD_3SEC_CNT   EQU     .237            ; 3秒長押し判定カウント（6フレーム×237≒3000ms）
FB_200MS_FRAMES EQU     .95             ; 合図点滅の200ms単位フレーム数
FB_150MS_FRAMES EQU     .71             ; 設定変更時の150ms消灯フレーム数

;------------------------------------------------------------------------------
; RAM定義（PIC12F629 汎用レジスタ：0x20～0x5F）
;------------------------------------------------------------------------------

        CBLOCK  0x20
W_TEMP
STATUS_TEMP
PWM_PHASE
PWM_DUTY
FRAME_FLAG
FRAME_DIV
FADE_INDEX
FADE_DIR
PERIOD_STEP
SLOPE_STEP
EDIT_MODE
FB_MODE
FB_SUB
FB_STEP
TICK_ACC
STEP_CNT
BTN_STATE
LAST_BTN
SEL_VALID
LONG_SAVED
SEL_HOLD_SUB
SEL_HOLD_CNT
SEL_COOLDOWN
UP_COOLDOWN
DN_COOLDOWN
OFF_FRAME_CNT
        ENDC

;------------------------------------------------------------------------------
; リセットベクタ
;------------------------------------------------------------------------------

        ORG     0x0000
        GOTO    INIT

;------------------------------------------------------------------------------
; 割り込み処理
;------------------------------------------------------------------------------

        ORG     0x0004

ISR

        ; W、STATUS退避（STATUS読み取り直後にBank 0へ切り替えてから保存）
        MOVWF   W_TEMP
        SWAPF   STATUS,W
        BCF     STATUS,RP0
        MOVWF   STATUS_TEMP

        ;--------------------------------------------------------------
        ; Timer0再設定（64段階PWM用高速周期：初期値234）
        ;--------------------------------------------------------------

        ; 4MHz: 割り込み応答約3cy + ISR先頭6cy + TMR0書込禁止2cy
        ; + (256-234)=22cy → 約33命令サイクルを狙う
        MOVLW   .234
        MOVWF   TMR0
        BCF     INTCON,T0IF

        ;--------------------------------------------------------------
        ; PWM位相更新（64段階：0～63）
        ;--------------------------------------------------------------

        INCF    PWM_PHASE,F

        MOVLW   .64
        SUBWF   PWM_PHASE,W

        BTFSS   STATUS,Z
        GOTO    PWM_COMPARE

        CLRF    PWM_PHASE
        BSF     FRAME_FLAG,0

;------------------------------------------------------------------------------
; PWM比較処理
;------------------------------------------------------------------------------

PWM_COMPARE

        ; PWM_PHASE >= PWM_DUTY なら消灯
        MOVF    PWM_DUTY,W
        SUBWF   PWM_PHASE,W

        BTFSC   STATUS,C
        GOTO    LED_OFF

;------------------------------------------------------------------------------
; LED点灯（GP4 HIGH）
;------------------------------------------------------------------------------

LED_ON

        BSF     GPIO,4
        GOTO    ISR_EXIT

;------------------------------------------------------------------------------
; LED消灯（GP4 LOW）
;------------------------------------------------------------------------------

LED_OFF

        BCF     GPIO,4

;------------------------------------------------------------------------------
; 割り込み終了
;------------------------------------------------------------------------------

ISR_EXIT

        ; STATUS復帰
        SWAPF   STATUS_TEMP,W
        MOVWF   STATUS

        ; W復帰
        SWAPF   W_TEMP,F
        SWAPF   W_TEMP,W

        RETFIE

;==============================================================================
; 初期化
;==============================================================================

        ORG     0x0030

INIT

        ; GPIO初期化（Bank 0）
        BCF     STATUS,RP0
        CLRF    GPIO

        ; コンパレータOFF（GP0～GP2をデジタルI/Oとして使用）
        MOVLW   0x07
        MOVWF   CMCON

        ; Bank 1へ切り替え
        BSF     STATUS,RP0

        ; 内部発振器の工場出荷時校正値（0x3FF）をOSCCALへ書き込み
        CALL    0x3FF
        MOVWF   OSCCAL

        ; I/O方向設定：GP0～GP3を入力(1)、GP4・GP5を出力(0)
        MOVLW   b'00001111'
        MOVWF   TRISIO

        ; GP0(PLUS)、GP1(MINUS)、GP2(SELECT)の内部プルアップを有効化
        MOVLW   b'00000111'
        MOVWF   WPU

        ; GPプルアップ有効(NOT_GPPU=0)、Timer0内部クロック(T0CS=0)、プリスケーラなし(PSA=1)
        MOVLW   b'00001000'
        MOVWF   OPTION_REG

        ; Bank 0へ戻す
        BCF     STATUS,RP0

        ; RAM初期化
        CLRF    PWM_PHASE
        CLRF    PWM_DUTY
        CLRF    FRAME_FLAG
        CLRF    FRAME_DIV
        CLRF    FADE_INDEX
        CLRF    FADE_DIR

        CLRF    EDIT_MODE
        CLRF    FB_MODE
        CLRF    FB_SUB
        CLRF    FB_STEP
        CLRF    TICK_ACC
        CLRF    STEP_CNT
        CLRF    BTN_STATE
        CLRF    LAST_BTN
        CLRF    SEL_VALID
        CLRF    LONG_SAVED
        CLRF    SEL_HOLD_SUB
        CLRF    SEL_HOLD_CNT
        CLRF    SEL_COOLDOWN
        CLRF    UP_COOLDOWN
        CLRF    DN_COOLDOWN
        CLRF    OFF_FRAME_CNT

        ; 初期ステップ値（中央値5）
        MOVLW   .5
        MOVWF   PERIOD_STEP
        MOVWF   SLOPE_STEP

        ; EEPROMから保存済み設定を読み込み
        CALL    LOAD_SETTINGS_EEPROM

        ; ウインカー周期の初期化
        CALL    RESET_WINKER_CYCLE

        ; Timer0初期値
        MOVLW   .234
        MOVWF   TMR0

        ; 割り込み許可
        BCF     INTCON,T0IF
        BSF     INTCON,T0IE
        BSF     INTCON,GIE

        GOTO    MAIN

;------------------------------------------------------------------------------
; PWMデューティテーブル（64段階二次曲線、ページ0内配置）
;------------------------------------------------------------------------------

DUTY_TABLE

        ADDWF   PCL,F

        RETLW   .0
        RETLW   .0
        RETLW   .1
        RETLW   .1
        RETLW   .1
        RETLW   .2
        RETLW   .2
        RETLW   .3
        RETLW   .3
        RETLW   .4
        RETLW   .4
        RETLW   .5
        RETLW   .5
        RETLW   .6
        RETLW   .6
        RETLW   .7
        RETLW   .8
        RETLW   .8
        RETLW   .9
        RETLW   .10
        RETLW   .10
        RETLW   .11
        RETLW   .12
        RETLW   .13
        RETLW   .14
        RETLW   .14
        RETLW   .15
        RETLW   .16
        RETLW   .17
        RETLW   .18
        RETLW   .19
        RETLW   .20
        RETLW   .21
        RETLW   .22
        RETLW   .23
        RETLW   .24
        RETLW   .25
        RETLW   .26
        RETLW   .27
        RETLW   .29
        RETLW   .30
        RETLW   .31
        RETLW   .32
        RETLW   .34
        RETLW   .35
        RETLW   .36
        RETLW   .38
        RETLW   .39
        RETLW   .41
        RETLW   .42
        RETLW   .44
        RETLW   .45
        RETLW   .47
        RETLW   .49
        RETLW   .50
        RETLW   .52
        RETLW   .54
        RETLW   .56
        RETLW   .57
        RETLW   .59
        RETLW   .61
        RETLW   .63
        RETLW   .64
        RETLW   .64

;------------------------------------------------------------------------------
; 周期分周テーブル（0～9：約500ms～1500ms相当、OFF 25%、ページ0内配置）
;------------------------------------------------------------------------------

PERIOD_TABLE

        ADDWF   PCL,F

        RETLW   .20
        RETLW   .24
        RETLW   .28
        RETLW   .33
        RETLW   .37
        RETLW   .42
        RETLW   .46
        RETLW   .50
        RETLW   .55
        RETLW   .59

;------------------------------------------------------------------------------
; 消灯フレーム数テーブル（25%相当、約2.112ms/フレーム）
;------------------------------------------------------------------------------

OFF_FRAME_TABLE

        ADDWF   PCL,F

        RETLW   .59
        RETLW   .71
        RETLW   .83
        RETLW   .98
        RETLW   .110
        RETLW   .125
        RETLW   .137
        RETLW   .149
        RETLW   .164
        RETLW   .176

;------------------------------------------------------------------------------
; メインループ
;------------------------------------------------------------------------------

MAIN

        BTFSS   FRAME_FLAG,0
        GOTO    MAIN

        BCF     FRAME_FLAG,0

        ; スイッチ入力監視（1フレーム＝約2.112ms想定）
        CALL    PROCESS_BUTTONS

        ; フィードバック（合図点滅）中か判定
        MOVF    FB_MODE,F
        BTFSS   STATUS,Z
        GOTO    MAIN_FEEDBACK

        ; ウインカースイッチ入力確認（GP3：HIGHでON、LOWで消灯待機）
        BTFSS   GPIO,3
        GOTO    WINKER_OFF_IDLE

        ; 通常の魂動ウインカー処理
        CALL    PROCESS_WINKER
        GOTO    MAIN

WINKER_OFF_IDLE

        ; ウインカーOFF時は消灯して周期先頭で待機
        CALL    RESET_WINKER_CYCLE
        GOTO    MAIN

MAIN_FEEDBACK

        CALL    PROCESS_FEEDBACK
        GOTO    MAIN

;------------------------------------------------------------------------------
; ウインカー周期進行処理
;------------------------------------------------------------------------------

PROCESS_WINKER

        ; 消灯フェーズ中はフレームカウントのみ行う
        MOVF    FADE_DIR,W
        XORLW   .2
        BTFSC   STATUS,Z
        GOTO    OFF_FRAME_PROC

        MOVLW   .128
        ADDWF   TICK_ACC,F

WINK_TICK_LOOP

        MOVF    PERIOD_STEP,W
        CALL    PERIOD_TABLE
        SUBWF   TICK_ACC,W
        BTFSS   STATUS,C
        RETURN

        MOVWF   TICK_ACC
        CALL    UPDATE_FADE
        GOTO    WINK_TICK_LOOP

OFF_FRAME_PROC

        CLRF    PWM_DUTY
        INCF    OFF_FRAME_CNT,F
        MOVF    PERIOD_STEP,W
        CALL    OFF_FRAME_TABLE
        SUBWF   OFF_FRAME_CNT,W
        BTFSS   STATUS,C
        RETURN

        GOTO    RESET_WINKER_CYCLE

;------------------------------------------------------------------------------
; フェード更新（上昇64段階・下降64段階・25%消灯の3フェーズ制御）
;------------------------------------------------------------------------------

UPDATE_FADE

        INCF    STEP_CNT,F

        MOVF    FADE_DIR,W
        BTFSC   STATUS,Z
        GOTO    FADE_UP

        XORLW   .1
        BTFSC   STATUS,Z
        GOTO    FADE_DOWN

        RETURN

FADE_UP

        ; 上昇ステップ待ち時間 = SLOPE_STEP + 2（P84 Ver.0.77準拠）
        MOVF    SLOPE_STEP,W
        ADDLW   .2
        SUBWF   STEP_CNT,W
        BTFSS   STATUS,C
        RETURN

        CLRF    STEP_CNT
        INCF    FADE_INDEX,F

        MOVLW   .64
        SUBWF   FADE_INDEX,W
        BTFSS   STATUS,Z
        GOTO    FADE_LOAD

        ; 上昇完了 -> 下降フェーズへ遷移
        MOVLW   .1
        MOVWF   FADE_DIR
        MOVLW   .63
        MOVWF   FADE_INDEX
        GOTO    FADE_LOAD

FADE_DOWN

        ; 下降ステップ待ち時間 = 16 - SLOPE_STEP（P84 Ver.0.77準拠）
        MOVF    SLOPE_STEP,W
        SUBLW   .16
        SUBWF   STEP_CNT,W
        BTFSS   STATUS,C
        RETURN

        CLRF    STEP_CNT

        MOVF    FADE_INDEX,F
        BTFSC   STATUS,Z
        GOTO    FADE_TO_OFF

        DECF    FADE_INDEX,F
        GOTO    FADE_LOAD

FADE_TO_OFF

        ; 下降完了 -> 周期残り25%の消灯フェーズへ遷移
        CLRF    STEP_CNT
        CLRF    OFF_FRAME_CNT
        MOVLW   .2
        MOVWF   FADE_DIR
        CLRF    PWM_DUTY
        RETURN

FADE_LOAD

        MOVF    FADE_INDEX,W
        CALL    DUTY_TABLE
        MOVWF   PWM_DUTY
        RETURN

;------------------------------------------------------------------------------
; ウインカー周期の先頭リセット
;------------------------------------------------------------------------------

RESET_WINKER_CYCLE

        CLRF    TICK_ACC
        CLRF    STEP_CNT
        CLRF    FRAME_DIV
        CLRF    OFF_FRAME_CNT
        CLRF    FADE_DIR
        CLRF    FADE_INDEX
        MOVF    FADE_INDEX,W
        CALL    DUTY_TABLE
        MOVWF   PWM_DUTY
        RETURN

;------------------------------------------------------------------------------
; 物理スイッチ入力処理（GP0:PLUS, GP1:MINUS, GP2:SELECT）
;------------------------------------------------------------------------------

PROCESS_BUTTONS

        ; チャタリング防止タイマの減算
        MOVF    SEL_COOLDOWN,F
        BTFSS   STATUS,Z
        DECF    SEL_COOLDOWN,F

        MOVF    UP_COOLDOWN,F
        BTFSS   STATUS,Z
        DECF    UP_COOLDOWN,F

        MOVF    DN_COOLDOWN,F
        BTFSS   STATUS,Z
        DECF    DN_COOLDOWN,F

        ; GPIO読み込み（アクティブLowを反転して押下=1に変換）
        COMF    GPIO,W
        ANDLW   b'00000111'
        MOVWF   BTN_STATE

        ;--------------------------------------------------------------
        ; GP2 : SELECTスイッチ処理（短押し：切替 / 3秒長押し：保存）
        ;--------------------------------------------------------------

        BTFSS   BTN_STATE,2
        GOTO    SEL_IS_RELEASED

        BTFSC   LAST_BTN,2
        GOTO    SEL_IS_HELD

        ; 押した瞬間
        MOVF    SEL_COOLDOWN,F
        BTFSS   STATUS,Z
        GOTO    CHECK_BTN_PLUS

        BSF     SEL_VALID,0
        CLRF    LONG_SAVED
        CLRF    SEL_HOLD_SUB
        CLRF    SEL_HOLD_CNT
        GOTO    CHECK_BTN_PLUS

SEL_IS_HELD

        ; 押し続けている間（3秒長押し判定）
        BTFSS   SEL_VALID,0
        GOTO    CHECK_BTN_PLUS

        BTFSC   LONG_SAVED,0
        GOTO    CHECK_BTN_PLUS

        INCF    SEL_HOLD_SUB,F
        MOVLW   .6
        SUBWF   SEL_HOLD_SUB,W
        BTFSS   STATUS,Z
        GOTO    CHECK_BTN_PLUS

        CLRF    SEL_HOLD_SUB
        INCF    SEL_HOLD_CNT,F
        MOVLW   HOLD_3SEC_CNT
        SUBWF   SEL_HOLD_CNT,W
        BTFSS   STATUS,Z
        GOTO    CHECK_BTN_PLUS

        ; 3秒経過：EEPROMへ保存して3回点滅合図を開始
        CALL    SAVE_SETTINGS_EEPROM
        BSF     LONG_SAVED,0
        MOVLW   .4
        MOVWF   FB_MODE
        CLRF    FB_SUB
        CLRF    FB_STEP
        GOTO    CHECK_BTN_PLUS

SEL_IS_RELEASED

        ; 離れている状態
        BTFSS   LAST_BTN,2
        GOTO    CHECK_BTN_PLUS

        ; 離した瞬間：長押し未実行なら短押しとしてモード切替
        BTFSS   SEL_VALID,0
        GOTO    SEL_REL_DONE

        BTFSC   LONG_SAVED,0
        GOTO    SEL_REL_DONE

        MOVLW   .1
        XORWF   EDIT_MODE,F
        MOVF    EDIT_MODE,W
        ADDLW   .1
        MOVWF   FB_MODE
        CLRF    FB_SUB
        CLRF    FB_STEP

SEL_REL_DONE

        CLRF    SEL_VALID
        MOVLW   DEBOUNCE_FRAMES
        MOVWF   SEL_COOLDOWN

        ;--------------------------------------------------------------
        ; GP0 : PLUSスイッチ処理
        ;--------------------------------------------------------------

CHECK_BTN_PLUS

        BTFSS   BTN_STATE,0
        GOTO    PLUS_IS_RELEASED

        BTFSC   LAST_BTN,0
        GOTO    CHECK_BTN_MINUS

        ; 押した瞬間
        MOVF    UP_COOLDOWN,F
        BTFSS   STATUS,Z
        GOTO    CHECK_BTN_MINUS

        BTFSC   EDIT_MODE,0
        GOTO    PLUS_EDIT_SLOPE

        ; 周期ステップを+1（上限9）
        MOVLW   .9
        SUBWF   PERIOD_STEP,W
        BTFSS   STATUS,C
        INCF    PERIOD_STEP,F
        GOTO    PLUS_TRIGGER_FB

PLUS_EDIT_SLOPE

        ; 傾斜ステップを+1（上限9）
        MOVLW   .9
        SUBWF   SLOPE_STEP,W
        BTFSS   STATUS,C
        INCF    SLOPE_STEP,F

PLUS_TRIGGER_FB

        MOVLW   .3
        MOVWF   FB_MODE
        CLRF    FB_SUB
        CLRF    FB_STEP
        MOVLW   DEBOUNCE_FRAMES
        MOVWF   UP_COOLDOWN
        GOTO    CHECK_BTN_MINUS

PLUS_IS_RELEASED

        BTFSS   LAST_BTN,0
        GOTO    CHECK_BTN_MINUS

        MOVLW   DEBOUNCE_FRAMES
        MOVWF   UP_COOLDOWN

        ;--------------------------------------------------------------
        ; GP1 : MINUSスイッチ処理
        ;--------------------------------------------------------------

CHECK_BTN_MINUS

        BTFSS   BTN_STATE,1
        GOTO    MINUS_IS_RELEASED

        BTFSC   LAST_BTN,1
        GOTO    BTN_PROC_EXIT

        ; 押した瞬間
        MOVF    DN_COOLDOWN,F
        BTFSS   STATUS,Z
        GOTO    BTN_PROC_EXIT

        BTFSC   EDIT_MODE,0
        GOTO    MINUS_EDIT_SLOPE

        ; 周期ステップを-1（下限0）
        MOVF    PERIOD_STEP,F
        BTFSS   STATUS,Z
        DECF    PERIOD_STEP,F
        GOTO    MINUS_TRIGGER_FB

MINUS_EDIT_SLOPE

        ; 傾斜ステップを-1（下限0）
        MOVF    SLOPE_STEP,F
        BTFSS   STATUS,Z
        DECF    SLOPE_STEP,F

MINUS_TRIGGER_FB

        MOVLW   .3
        MOVWF   FB_MODE
        CLRF    FB_SUB
        CLRF    FB_STEP
        MOVLW   DEBOUNCE_FRAMES
        MOVWF   DN_COOLDOWN
        GOTO    BTN_PROC_EXIT

MINUS_IS_RELEASED

        BTFSS   LAST_BTN,1
        GOTO    BTN_PROC_EXIT

        MOVLW   DEBOUNCE_FRAMES
        MOVWF   DN_COOLDOWN

BTN_PROC_EXIT

        MOVF    BTN_STATE,W
        MOVWF   LAST_BTN
        RETURN

;------------------------------------------------------------------------------
; フィードバック（合図点滅）処理
;   FB_MODE 1 : 周期モード切替（1回点滅）
;   FB_MODE 2 : 傾斜モード切替（2回点滅）
;   FB_MODE 3 : ＋/－設定変更（150ms強制消灯＆再スタート）
;   FB_MODE 4 : EEPROM保存完了（3回点滅）
;------------------------------------------------------------------------------

PROCESS_FEEDBACK

        MOVF    FB_MODE,W
        XORLW   .3
        BTFSC   STATUS,Z
        GOTO    FB_MODE3_PROC

        ; FB_STEPに応じてON/OFFを決定（200ms単位）
        ; FB_STEP == 2 : 1回目点灯
        MOVF    FB_STEP,W
        XORLW   .2
        BTFSC   STATUS,Z
        GOTO    FB_LED_ON

        ; FB_STEP == 4 : FB_MODE 2または4のとき2回目点灯
        MOVF    FB_STEP,W
        XORLW   .4
        BTFSS   STATUS,Z
        GOTO    FB_CHECK_BLINK3

        MOVF    FB_MODE,W
        XORLW   .1
        BTFSS   STATUS,Z
        GOTO    FB_LED_ON
        GOTO    FB_LED_OFF

FB_CHECK_BLINK3

        ; FB_STEP == 6 : FB_MODE 4のとき3回目点灯
        MOVF    FB_STEP,W
        XORLW   .6
        BTFSS   STATUS,Z
        GOTO    FB_LED_OFF

        MOVF    FB_MODE,W
        XORLW   .4
        BTFSC   STATUS,Z
        GOTO    FB_LED_ON

FB_LED_OFF

        CLRF    PWM_DUTY
        GOTO    FB_STEP_ADVANCE

FB_LED_ON

        MOVLW   .64
        MOVWF   PWM_DUTY

FB_STEP_ADVANCE

        INCF    FB_SUB,F
        MOVLW   FB_200MS_FRAMES
        SUBWF   FB_SUB,W
        BTFSS   STATUS,Z
        RETURN

        CLRF    FB_SUB
        INCF    FB_STEP,F

        ; 終了判定：FB_MODE 1は6単位(1200ms)、2は8単位(1600ms)、4は10単位(2000ms)
        MOVF    FB_MODE,W
        XORLW   .1
        BTFSC   STATUS,Z
        GOTO    FB_LIM_6

        MOVF    FB_MODE,W
        XORLW   .2
        BTFSC   STATUS,Z
        GOTO    FB_LIM_8

        MOVLW   .10
        GOTO    FB_LIM_CHECK

FB_LIM_6

        MOVLW   .6
        GOTO    FB_LIM_CHECK

FB_LIM_8

        MOVLW   .8

FB_LIM_CHECK

        SUBWF   FB_STEP,W
        BTFSS   STATUS,Z
        RETURN

        GOTO    FB_FINISH

FB_MODE3_PROC

        ; 150ms強制消灯
        CLRF    PWM_DUTY
        INCF    FB_SUB,F
        MOVLW   FB_150MS_FRAMES
        SUBWF   FB_SUB,W
        BTFSS   STATUS,Z
        RETURN

FB_FINISH

        CLRF    FB_MODE
        CALL    RESET_WINKER_CYCLE
        RETURN

;------------------------------------------------------------------------------
; 内蔵EEPROMからの設定読み込み（PIC12F629：EEADR/EEDATAはBank 1）
;------------------------------------------------------------------------------

LOAD_SETTINGS_EEPROM

        ; アドレス0x00のマジックナンバー確認
        MOVLW   0x00
        CALL    EEPROM_READ_BYTE
        XORLW   EEPROM_MAGIC
        BTFSS   STATUS,Z
        RETURN

        ; アドレス0x01からPERIOD_STEPを読み込み
        MOVLW   0x01
        CALL    EEPROM_READ_BYTE
        MOVWF   PERIOD_STEP
        MOVLW   .10
        SUBWF   PERIOD_STEP,W
        BTFSS   STATUS,C
        GOTO    LOAD_SLOPE_EEPROM

        MOVLW   .5
        MOVWF   PERIOD_STEP

LOAD_SLOPE_EEPROM

        ; アドレス0x02からSLOPE_STEPを読み込み
        MOVLW   0x02
        CALL    EEPROM_READ_BYTE
        MOVWF   SLOPE_STEP
        MOVLW   .10
        SUBWF   SLOPE_STEP,W
        BTFSS   STATUS,C
        RETURN

        MOVLW   .5
        MOVWF   SLOPE_STEP
        RETURN

EEPROM_READ_BYTE

        BSF     STATUS,RP0
        MOVWF   EEADR
        BSF     EECON1,RD
        MOVF    EEDATA,W
        BCF     STATUS,RP0
        RETURN

;------------------------------------------------------------------------------
; 内蔵EEPROMへの設定保存（4段階安全書き込み / PIC12F629 Bank 1対応）
;------------------------------------------------------------------------------

SAVE_SETTINGS_EEPROM

        ; 1. アドレス0x00：マジックナンバーを一旦無効化（0x00書き込み）
        BSF     STATUS,RP0
        MOVLW   0x00
        MOVWF   EEADR
        CLRF    EEDATA
        CALL    EEPROM_WRITE_EXEC

        ; 2. アドレス0x01：PERIOD_STEP書き込み
        BCF     STATUS,RP0
        MOVF    PERIOD_STEP,W
        BSF     STATUS,RP0
        MOVWF   EEDATA
        MOVLW   0x01
        MOVWF   EEADR
        CALL    EEPROM_WRITE_EXEC

        ; 3. アドレス0x02：SLOPE_STEP書き込み
        BCF     STATUS,RP0
        MOVF    SLOPE_STEP,W
        BSF     STATUS,RP0
        MOVWF   EEDATA
        MOVLW   0x02
        MOVWF   EEADR
        CALL    EEPROM_WRITE_EXEC

        ; 4. アドレス0x00：マジックナンバー書き込み（最後に有効化）
        BSF     STATUS,RP0
        MOVLW   0x00
        MOVWF   EEADR
        MOVLW   EEPROM_MAGIC
        MOVWF   EEDATA
        CALL    EEPROM_WRITE_EXEC

        RETURN

EEPROM_WRITE_EXEC

        BSF     STATUS,RP0
        BSF     EECON1,WREN
        BCF     INTCON,GIE
        MOVLW   0x55
        MOVWF   EECON2
        MOVLW   0xAA
        MOVWF   EECON2
        BSF     EECON1,WR
        BSF     INTCON,GIE

EEPROM_WAIT_WR

        BTFSC   EECON1,WR
        GOTO    EEPROM_WAIT_WR

        BCF     EECON1,WREN
        BCF     STATUS,RP0
        RETURN

        END