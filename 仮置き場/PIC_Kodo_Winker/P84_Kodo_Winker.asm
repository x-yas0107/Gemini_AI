;==============================================================================
; プロジェクト : P84_Kodo_Winker
; ファイル     : P84_Kodo_Winker.asm
; 対象CPU      : PIC16F84A
; アセンブラ   : MPASM
;
; Ver.0.77
;
; 【変更内容】
;   ・64段階PWMおよび二次曲線フェードを維持
;   ・実機RA2測定値（HIGH/LOW各32.5us）に合わせて時間基準を修正
;   ・上昇/下降待ち時間を修正し、1周期を1280ティック（点灯1152、消灯128）に整合
;   ・周期、ボタン、合図点滅の時間定数を実機フレーム周期に合わせて修正
;   ・EEPROM保存、スイッチ処理、RA0/RA2出力は維持
;
; 【I/O設定】
;   RA0 : LED_PWM出力
;   RA1 : 未使用
;   RA2 : 割り込み周期測定用トグル出力
;   RA3 : 未使用
;   RA4 : 未使用
;   RB0 : SELECT（短押し：周期/傾斜切替、3秒長押し：EEPROM保存）
;   RB1 : PLUS（設定値UP）
;   RB2 : MINUS（設定値DOWN）
;   RB3～RB5 : 未使用
;   RB6 : ICSPCLK
;   RB7 : ICSPDAT
;
; 【発振】
;   RC発振モード（実機）
;
; 【Timer0】
;   内部クロック
;   プリスケーラなし
;   初期値217、RA2実測HIGH/LOW各32.5us
;   PWMフレーム周期：約2.08ms（64割り込み、約481Hz）
;
; 【変更履歴】
;   Ver.0.1  : 初期PWM試作
;   Ver.0.2  : バンク切り替え処理修正
;   Ver.0.3  : PWM・フェード処理整理
;   Ver.0.4  : RA2測定出力追加
;   Ver.0.5  : 割り込みベクタからのジャンプ削除
;   Ver.0.6  : Timer0再設定位置変更、RA2トグル方式
;   Ver.0.7  : RA2トグルタイミング統一、状態変数初期化
;   Ver.0.71 : スイッチ操作、周期/傾斜10段階制御、合図点滅、EEPROM保存、90%点灯波形を実装
;   Ver.0.72 : 合図点滅の終了判定バグ修正、EEPROM保存順序の安全化
;   Ver.0.73 : EEPROM保存前のマジックナンバー無効化処理追加、FADE_TO_OFF時のSTEP_CNTクリア明記
;   Ver.0.74 : ISR退避処理のBank 0切替順序を修正（STATUS退避直後にBank 0選択）
;   Ver.0.75 : PWMおよびフェードテーブルを32段階へ高分解能化、Timer0初期値を193へ最適化
;   Ver.0.76 : フェードおよびPWMを64段階へ高精度化、Timer0初期値を217へ最適化
;   Ver.0.77 : 実機フレーム周期に合わせてフェード時間・周期・操作時間定数を修正
;
; 【注意】
;   実機RA2測定値を基準に時間定数を調整。Ver.0.77の最終動作確認は未完了。
;==============================================================================

        LIST    P=16F84A
        #include <P16F84A.INC>

        __CONFIG _CP_OFF & _WDT_OFF & _PWRTE_ON & _RC_OSC

;------------------------------------------------------------------------------
; 定数定義
;------------------------------------------------------------------------------

EEPROM_MAGIC    EQU     0xA5            ; EEPROMデータ有効確認用マジックナンバー
DEBOUNCE_FRAMES EQU     .120            ; チャタリング防止期間（約250ms）
HOLD_3SEC_CNT   EQU     .240            ; 3秒長押し判定カウント（6フレーム×240≒3000ms）
FB_200MS_FRAMES EQU     .96             ; 合図点滅の200ms単位フレーム数
FB_150MS_FRAMES EQU     .72             ; 設定変更時の150ms消灯フレーム数

;------------------------------------------------------------------------------
; RAM定義
;------------------------------------------------------------------------------

        CBLOCK  0x0C
W_TEMP
STATUS_TEMP
PWM_PHASE
PWM_DUTY
FRAME_FLAG
FRAME_DIV
FADE_INDEX
FADE_DIR
MEAS_STATE
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
        ; Timer0再設定（64段階PWM用高速周期：初期値217）
        ;--------------------------------------------------------------

        MOVLW   .217
        MOVWF   TMR0
        BCF     INTCON,T0IF

        ;--------------------------------------------------------------
        ; RA2測定用トグル
        ;
        ; HIGH側、LOW側ともに
        ; トグル命令まで3命令サイクル
        ;
        ; トグル後の処理も同じサイクル数に調整
        ;--------------------------------------------------------------

        BTFSC   MEAS_STATE,0
        GOTO    MEAS_HIGH

        ; LOW側のタイミング調整
        NOP

MEAS_LOW

        BCF     PORTA,2
        BSF     MEAS_STATE,0
        GOTO    MEAS_DONE

MEAS_HIGH

        BSF     PORTA,2
        BCF     MEAS_STATE,0
        NOP
        NOP

MEAS_DONE

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
; LED点灯
;------------------------------------------------------------------------------

LED_ON

        BSF     PORTA,0
        GOTO    ISR_EXIT

;------------------------------------------------------------------------------
; LED消灯
;------------------------------------------------------------------------------

LED_OFF

        BCF     PORTA,0

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

        ORG     0x0040

INIT

        ; PORT初期化
        BCF     STATUS,RP0
        CLRF    PORTA
        CLRF    PORTB

        ; I/O設定
        BSF     STATUS,RP0

        ; RA0、RA2出力
        MOVLW   b'00011010'
        MOVWF   TRISA

        ; PORTB全ピン入力
        MOVLW   b'11111111'
        MOVWF   TRISB

        ; RB内部プルアップ有効
        ; Timer0内部クロック
        ; プリスケーラなし
        MOVLW   b'00001000'
        MOVWF   OPTION_REG

        BCF     STATUS,RP0

        ; RAM初期化
        CLRF    PWM_PHASE
        CLRF    PWM_DUTY
        CLRF    FRAME_FLAG
        CLRF    FRAME_DIV
        CLRF    FADE_INDEX
        CLRF    FADE_DIR
        CLRF    MEAS_STATE
        BSF     MEAS_STATE,0

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

        ; 初期ステップ値（中央値5）
        MOVLW   .5
        MOVWF   PERIOD_STEP
        MOVWF   SLOPE_STEP

        ; EEPROMから保存済み設定を読み込み
        CALL    LOAD_SETTINGS_EEPROM

        ; ウインカー周期の初期化
        CALL    RESET_WINKER_CYCLE

        ; Timer0初期値
        MOVLW   .217
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
; 周期分周テーブル（0～9：約500ms～1500ms、実機測定基準、ページ0内配置）
;------------------------------------------------------------------------------

PERIOD_TABLE

        ADDWF   PCL,F

        RETLW   .24
        RETLW   .29
        RETLW   .35
        RETLW   .40
        RETLW   .45
        RETLW   .51
        RETLW   .56
        RETLW   .61
        RETLW   .67
        RETLW   .72

;------------------------------------------------------------------------------
; メインループ
;------------------------------------------------------------------------------

MAIN

        BTFSS   FRAME_FLAG,0
        GOTO    MAIN

        BCF     FRAME_FLAG,0

        ; スイッチ入力監視（1フレーム＝約2.08ms周期）
        CALL    PROCESS_BUTTONS

        ; フィードバック（合図点滅）中か判定
        MOVF    FB_MODE,F
        BTFSS   STATUS,Z
        GOTO    MAIN_FEEDBACK

        ; 通常の魂動ウインカー処理
        CALL    PROCESS_WINKER
        GOTO    MAIN

MAIN_FEEDBACK

        CALL    PROCESS_FEEDBACK
        GOTO    MAIN

;------------------------------------------------------------------------------
; ウインカー周期進行処理
;------------------------------------------------------------------------------

PROCESS_WINKER

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

;------------------------------------------------------------------------------
; フェード更新（上昇64段階・下降64段階・10%消灯の3フェーズ制御）
;------------------------------------------------------------------------------

UPDATE_FADE

        INCF    STEP_CNT,F

        MOVF    FADE_DIR,W
        BTFSC   STATUS,Z
        GOTO    FADE_UP

        XORLW   .1
        BTFSC   STATUS,Z
        GOTO    FADE_DOWN

        GOTO    FADE_OFF_WAIT

FADE_UP

        ; 上昇ステップ待ち時間 = SLOPE_STEP + 2
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

        ; 下降ステップ待ち時間 = 16 - SLOPE_STEP
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

        ; 下降完了 -> 周期残り10%の消灯フェーズへ遷移（STEP_CNTを明示的に0へリセット）
        CLRF    STEP_CNT
        MOVLW   .2
        MOVWF   FADE_DIR
        CLRF    PWM_DUTY
        RETURN

FADE_OFF_WAIT

        ; 消灯フェーズ（128ティック＝周期全体の10%）
        CLRF    PWM_DUTY
        MOVLW   .128
        SUBWF   STEP_CNT,W
        BTFSS   STATUS,C
        RETURN

        ; 1周期完了 -> 次の周期へリセット
        GOTO    RESET_WINKER_CYCLE

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
        CLRF    FADE_DIR
        CLRF    FADE_INDEX
        MOVF    FADE_INDEX,W
        CALL    DUTY_TABLE
        MOVWF   PWM_DUTY
        RETURN

;------------------------------------------------------------------------------
; 物理スイッチ入力処理（RB0:SELECT, RB1:PLUS, RB2:MINUS）
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

        ; PORTB読み込み（アクティブLowを反転して押下=1に変換）
        COMF    PORTB,W
        ANDLW   b'00000111'
        MOVWF   BTN_STATE

        ;--------------------------------------------------------------
        ; RB0 : SELECTスイッチ処理（短押し：切替 / 3秒長押し：保存）
        ;--------------------------------------------------------------

        BTFSS   BTN_STATE,0
        GOTO    SEL_IS_RELEASED

        BTFSC   LAST_BTN,0
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
        BTFSS   LAST_BTN,0
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
        ; RB1 : PLUSスイッチ処理
        ;--------------------------------------------------------------

CHECK_BTN_PLUS

        BTFSS   BTN_STATE,1
        GOTO    PLUS_IS_RELEASED

        BTFSC   LAST_BTN,1
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

        BTFSS   LAST_BTN,1
        GOTO    CHECK_BTN_MINUS

        MOVLW   DEBOUNCE_FRAMES
        MOVWF   UP_COOLDOWN

        ;--------------------------------------------------------------
        ; RB2 : MINUSスイッチ処理
        ;--------------------------------------------------------------

CHECK_BTN_MINUS

        BTFSS   BTN_STATE,2
        GOTO    MINUS_IS_RELEASED

        BTFSC   LAST_BTN,2
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

        BTFSS   LAST_BTN,2
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
; 内蔵EEPROMからの設定読み込み
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

        BCF     STATUS,RP0
        MOVWF   EEADR
        BSF     STATUS,RP0
        BSF     EECON1,RD
        BCF     STATUS,RP0
        MOVF    EEDATA,W
        RETURN

;------------------------------------------------------------------------------
; 内蔵EEPROMへの設定保存（4段階安全書き込み）
;------------------------------------------------------------------------------

SAVE_SETTINGS_EEPROM

        ; 1. アドレス0x00：マジックナンバーを一旦無効化（0x00書き込み）
        MOVLW   0x00
        MOVWF   EEADR
        CLRF    EEDATA
        CALL    EEPROM_WRITE_EXEC

        ; 2. アドレス0x01：PERIOD_STEP書き込み
        MOVLW   0x01
        MOVWF   EEADR
        MOVF    PERIOD_STEP,W
        MOVWF   EEDATA
        CALL    EEPROM_WRITE_EXEC

        ; 3. アドレス0x02：SLOPE_STEP書き込み
        MOVLW   0x02
        MOVWF   EEADR
        MOVF    SLOPE_STEP,W
        MOVWF   EEDATA
        CALL    EEPROM_WRITE_EXEC

        ; 4. アドレス0x00：マジックナンバー書き込み（最後に有効化）
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