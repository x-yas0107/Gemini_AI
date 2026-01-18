;====================================================================
; プロジェクト名: PIC16F84A MDI IGNITION SYSTEM [Grand Finale]
; バージョン	  : 8.00 (The Final Masterpiece)
; 作成日	  : 2026/01/18
; 共同開発  : Gemini_AI & yas
;
; 【概要】
; エンジンの点火時期と回数を制御しながら、OLED画面に回転数を
; リアルタイム表示する「デジタル・イグニッション・システム」です。
;
; 【ハードウェア配線図 (I/O MAP)】
;
;                   PIC16F84A (18ピンDIPパッケージ)
;                 +---------\/---------+
;  [回転数UP] RA2 -| 1 (入力)    (出力) 18 |- RA1 [OLED SCL]
;  [回転数DW] RA3 -| 2 (入力)    (出力) 17 |- RA0 [OLED SDA]
;            RA4 -| 3 (未使用)        16 |- OSC1 [20MHz]
;     [リセット] MCLR-| 4 (入力)          15 |- OSC2 [20MHz]
;      [GND] Vss -| 5 (電源-)  (電源+) 14 |- Vdd [+5V]
;   [点火信号] RB0 -| 6 (出力)   (未使用) 13 |- RB7
; [電源SW ON] RB1 -| 7 (入力)    (出力) 12 |- RB6 [LED 高回転]
;[電源SW OFF] RB2 -| 8 (入力)    (出力) 11 |- RB5 [LED 中回転]
;   [電源ランプ] RB3 -| 9 (出力)    (出力) 10 |- RB4 [LED 低回転]
;                 +--------------------+
;
;  ★ 入力のルール (負論理)
;     スイッチを押すと「0V (GND)」につながるように配線してください。
;     (ピンと+5Vの間に10kΩの抵抗を入れて、普段は+5Vにしておくこと！)
;
;  ★ 出力のルール (正論理)
;     ONになると「+5V」が出ます。LEDは抵抗を通してつないでください。
;====================================================================

    ; --- おまじない (使用するPICの宣言) ---
    LIST      P=16F84A
    INCLUDE   "P16F84A.INC"
    
    ; --- コンフィグ設定 (PICの基本動作設定) ---
    ; _HS_OSC   : 20MHzという速い水晶を使います
    ; _WDT_OFF  : 番犬(暴走監視)は寝かせておきます
    ; _PWRTE_ON : 電源ON時のふらつきを待ちます
    ; _CP_OFF   : プログラムの中身を隠しません
    __CONFIG  _HS_OSC & _WDT_OFF & _PWRTE_ON & _CP_OFF

    ; バンク切り替えの警告メッセージを消す (うるさいので)
    ERRORLEVEL -302 

;====================================================================
; 【メモリ割り当て表】 (RAM: 0x0C?)
; 変数という「数値を入れておく箱」に名前をつけます
;====================================================================
    CBLOCK  0x0C
        ; --- エンジンの状態管理 ---
        RPM_L, RPM_H        ; 今の回転数 (下8桁、上8桁)
        MDI_COUNT           ; 「あと何回点火するか」の回数券
        W_DWELL_H, W_DWELL_L ; コイルに電気を流す時間を計るタイマー
        DELAY_CNT           ; 次の点火までの「待ち時間」タイマー

        ; --- 画面表示のための道具 ---
        I2C_DATA, BIT_CNT   ; 通信で送るデータと、ビット数のカウント
        DISP_STATE          ; 「今、画面のどこを描いてるか」の番号 (0?14)
        LOOP_PAGE, LOOP_COL ; 繰り返し処理用のカウンター
        EXP_BUFF, FONT_PTR_L ; 文字を拡大するときの計算用紙
        BAR_LEN, BAR_TEMP   ; バーグラフの長さ(cm)と、描く残り長さ
        
        ; --- 数字の計算用 ---
        BCD_1000, BCD_100   ; 1000の位、100の位...
        BCD_10, BCD_1       ; 10の位、1の位
        NUM_H, NUM_L        ; 計算途中の数字置き場

        ; --- 制御用のタイマー ---
        W_REC_CNT, W_I2C    ; ちょっと待つためのタイマー
        W_MS_OUTER, W_MS_INNER ; 長ーく待つためのタイマー
        BTN_TIMER           ; ボタン連打を防ぐ「クールダウン」タイマー
    ENDC

;====================================================================
; 【ピンのあだ名付け】
; 番号で呼ぶと間違えるので、役割で呼べるようにします
;====================================================================
#DEFINE SDA       PORTA, 0  ; 画面へのデータ線
#DEFINE SCL       PORTA, 1  ; 画面への合図線
#DEFINE BTN_UP    PORTA, 2  ; UPボタン (押すと0V)
#DEFINE BTN_DW    PORTA, 3  ; DOWNボタン (押すと0V)

#DEFINE IGN_PIN   PORTB, 0  ; 点火信号 (ここから火花指令が出る！)
#DEFINE SW_PWR_ON PORTB, 1  ; 電源ONボタン (押すと0V)
#DEFINE SW_PWR_OFF PORTB, 2 ; 電源OFFボタン (押すと0V)
#DEFINE LED_POWER PORTB, 3  ; 電源が入っていると光るLED
#DEFINE LED_LO    PORTB, 4  ; 低回転で光るLED
#DEFINE LED_MID   PORTB, 5  ; 中回転で光るLED
#DEFINE LED_HI    PORTB, 6  ; 高回転で光るLED

;====================================================================
; 【スタート地点】
; 電源を入れた瞬間、PICはここ(0番地)から走り出します
;====================================================================
    ORG     0x000
    GOTO    INIT            ; 準備運動(INIT)へジャンプ！

;====================================================================
; 【準備運動 (初期化)】
; 最初に1回だけ行う設定です
;====================================================================
INIT
    ; --- 足の役割決め (入力か？出力か？) ---
    BSF     STATUS, RP0     ; 設定用の裏ページ(バンク1)を開く
    
    ; PORTAの設定: 0=出力(出す), 1=入力(見る)
    ; RA2,3,4は入力、他は出力
    MOVLW   b'00011100'     
    MOVWF   TRISA
    
    ; PORTBの設定
    ; RB1(ON), RB2(OFF)ボタンは入力、他は全部出力
    MOVLW   b'00000110'
    MOVWF   TRISB
    
    BCF     STATUS, RP0     ; 表ページ(バンク0)に戻る

    ; --- いったん全部消す ---
    CLRF    PORTA
    CLRF    PORTB           ; LEDも点火も全部OFF！
    
    ; --- 画面の電源が入るのを待つ ---
    CALL    WAIT_500MS      ; 0.5秒じっと待つ
    CALL    I2C_BUS_RECOVERY ; 通信線の準備運動

    ; --- 待機モードへ ---
    ; いきなり動かず、ONボタンが押されるのを待つ部屋へ行きます
    GOTO    MODE_STANDBY

;====================================================================
; 【待機モード (電源OFFの状態)】
; 電源は繋がってるけど、寝ているフリをする場所です
;====================================================================
MODE_STANDBY
    CLRF    PORTB           ; LEDも何もかも消す
    
    ; 画面を真っ暗にする
    CALL    OLED_INITIALIZE_FINAL
    CALL    OLED_CLEAR_ALL
    
STANDBY_LOOP
    ; 「ONスイッチ」が押されるのを監視します
    ; SW_PWR_ON は押すと 0(Low) になります
    BTFSC   SW_PWR_ON       ; 0(押された)ならスキップ、1(まだ)なら次へ
    GOTO    STANDBY_LOOP    ; まだ押されてないなら、ここでグルグル待つ
    
    ; --- 押された！起きろ！ ---
    CALL    WAIT_50MS       ; スイッチの振動(チャタリング)が収まるのを待つ
    
    ; 変数をリセット (0回転からスタート)
    CLRF    RPM_L
    CLRF    RPM_H
    CLRF    DISP_STATE      ; 画面描画の準備
    CLRF    BTN_TIMER
    
    BSF     LED_POWER       ; 「起きたよ！」の電源LED点灯
    
    ; メインのお仕事へ移動！
    GOTO    MAIN_RUN

;====================================================================
; 【メインモード (電源ONの状態)】
; ここでエンジンを制御し続けます
;====================================================================
MAIN_RUN
    ; --- 1. OFFボタンのチェック ---
    ; 「もう寝ていい？」と確認します
    BTFSS   SW_PWR_OFF      ; OFFボタンが押された(0)ならスキップ
    GOTO    SHUTDOWN_SEQ    ; 終了作業へ

    ; --- 2. LEDを光らせる ---
    ; 回転数に合わせてピカピカさせます
    CALL    UPDATE_LEDS

    ; --- 3. エンジン点火制御 ---
    ; 「今だ！火花を散らせ！」と命令します
    CALL    FIRE_SEQUENCE_MDI

    ; --- 4. UP/DOWNボタン処理 ---
    ; ボタンが押されたら回転数を変えます
    MOVF    BTN_TIMER, F    ; タイマー待ち中？
    BTFSC   STATUS, Z       ; 0なら
    CALL    CHECK_BUTTONS   ; ボタンを見に行く
    
    ; タイマーを減らす (待ち時間を消化)
    MOVF    BTN_TIMER, F
    BTFSS   STATUS, Z
    DECF    BTN_TIMER, F

    ; --- 5. 画面描画 ---
    ; 画面をちょっとだけ更新します
    CALL    DISP_TASK_MANAGER
    
    ; 最初に戻って繰り返す (超高速ループ！)
    GOTO    MAIN_RUN

; --- 終了作業 ---
SHUTDOWN_SEQ
    CALL    WAIT_50MS       ; 最後の振動待ち
    GOTO    MODE_STANDBY    ; 待機モード(寝室)へ戻る

;====================================================================
; 【LEDイルミネーション制御】
;====================================================================
UPDATE_LEDS
    ; いったん回転LEDを全部消す
    BCF     LED_LO
    BCF     LED_MID
    BCF     LED_HI

    ; 0回転 (停止中) なら -> LOを点灯
    MOVF    RPM_H, W
    IORWF   RPM_L, W
    BTFSC   STATUS, Z
    GOTO    LIGHT_LO

    ; 4096回転以上なら -> HIを点灯
    MOVLW   0x10
    SUBWF   RPM_H, W
    BTFSC   STATUS, C
    GOTO    LIGHT_HI

    ; 2048回転以上なら -> MIDを点灯
    MOVLW   0x08
    SUBWF   RPM_H, W
    BTFSC   STATUS, C
    GOTO    LIGHT_MID

    ; それ以外 (低い回転) -> LOを点灯
    GOTO    LIGHT_LO

LIGHT_LO
    BSF     LED_LO
    RETURN
LIGHT_MID
    BSF     LED_MID
    RETURN
LIGHT_HI
    BSF     LED_HI
    RETURN

;====================================================================
; 【MDI 点火司令塔】
; ここがエンジンの心臓部です！
;====================================================================
FIRE_SEQUENCE_MDI
    ; ★安全装置: 0回転なら点火しない！
    MOVF    RPM_H, F
    BTFSS   STATUS, Z       ; 0じゃないならOK
    GOTO    MDI_CHECK
    MOVF    RPM_L, F
    BTFSS   STATUS, Z       ; 0じゃないならOK
    GOTO    MDI_CHECK
    RETURN                  ; 0回転なので何もしないで帰る

MDI_CHECK
    ; 回転数を見て「何回叩くか」を決める
    MOVLW   0x10
    SUBWF   RPM_H, W
    BTFSC   STATUS, C
    GOTO    SET_1_SHOT      ; 高回転なら1発！
    MOVLW   0x08
    SUBWF   RPM_H, W
    BTFSC   STATUS, C
    GOTO    SET_2_SHOT      ; 中回転なら2発！
    GOTO    SET_3_SHOT      ; 低回転なら3発！

SET_1_SHOT
    MOVLW   d'1'
    MOVWF   MDI_COUNT
    GOTO    MDI_LOOP
SET_2_SHOT
    MOVLW   d'2'
    MOVWF   MDI_COUNT
    GOTO    MDI_LOOP
SET_3_SHOT
    MOVLW   d'3'
    MOVWF   MDI_COUNT
    
MDI_LOOP
    CALL    SINGLE_SPARK    ; バチッ！！
    DECFSZ  MDI_COUNT, F    ; 回数券を1枚破る。0になった？
    GOTO    MDI_INTERVAL    ; まだあるなら、ちょっと待って次へ
    
    ; 全部打ち終わった！
    ; エンジンのリズムに合わせて休憩する
    CALL    CYCLE_DELAY
    RETURN

MDI_INTERVAL
    CALL    WAIT_1MS        ; 連射の間のチャージ時間
    GOTO    MDI_LOOP

; --- エンジンのリズムを作る ---
; 回転数が低いほど、長く休みます (ドッ...ドッ...ドッ...)
CYCLE_DELAY
    MOVF    RPM_H, W
    SUBLW   0x24            ; 36から今の回転レベルを引く
    BTFSS   STATUS, C       ; もしマイナスになったら(超高回転)
    RETURN                  ; 休みなし！
    MOVWF   DELAY_CNT       ; 結果をタイマーにセット
    MOVF    DELAY_CNT, F
    BTFSC   STATUS, Z
    RETURN
DELAY_LOOP
    CALL    WAIT_1MS
    CALL    WAIT_1MS
    DECFSZ  DELAY_CNT, F
    GOTO    DELAY_LOOP
    RETURN

; --- 1回分の点火動作 ---
SINGLE_SPARK
    BSF     IGN_PIN         ; 通電開始！ (コイルにエネルギーを貯める)
    MOVLW   d'20'           ; 少し待つ(ドウェル時間)
    MOVWF   W_DWELL_H
SP_D1
    MOVLW   d'165'
    MOVWF   W_DWELL_L
SP_D2
    DECFSZ  W_DWELL_L, F
    GOTO    SP_D2
    DECFSZ  W_DWELL_H, F
    GOTO    SP_D1
    
    BCF     IGN_PIN         ; 遮断！！ (ここで高電圧が発生して火花が飛ぶ)
    RETURN

;====================================================================
; 【ボタン操作の受付】
;====================================================================
CHECK_BUTTONS
    ; ボタンは「押すと0(Low)」になる
    
    BTFSS   BTN_UP          ; UPボタンは0(押された)か？
    GOTO    DO_INC          ; Yes -> 増やす処理へ
    
    BTFSS   BTN_DW          ; DOWNボタンは0(押された)か？
    GOTO    DO_DEC          ; Yes -> 減らす処理へ
    
    RETURN

DO_INC
    ; 8000回転以上にはしない (リミッター)
    MOVLW   0x1F
    SUBWF   RPM_H, W
    BTFSC   STATUS, C
    GOTO    BTN_DONE        ; 上限なので無視
    
    ; 100回転増やす
    MOVLW   d'100'
    ADDWF   RPM_L, F
    BTFSC   STATUS, C       ; 繰り上がりした？
    INCF    RPM_H, F        ; 上の桁を+1
    GOTO    BTN_SET_TIMER

DO_DEC
    ; 0回転以下にはしない
    MOVF    RPM_H, F
    BTFSS   STATUS, Z
    GOTO    DEC_EXEC        ; まだ減らせる
    MOVLW   d'100'
    SUBWF   RPM_L, W
    BTFSS   STATUS, C       ; 引くとマイナスになる？
    GOTO    FORCE_ZERO      ; 0にする
DEC_EXEC
    ; 100回転減らす
    MOVLW   d'100'
    SUBWF   RPM_L, F
    BTFSS   STATUS, C       ; 繰り下がりした？
    DECF    RPM_H, F        ; 上の桁を-1
    GOTO    BTN_SET_TIMER
FORCE_ZERO
    CLRF    RPM_L
    CLRF    RPM_H

BTN_SET_TIMER
    ; 次のボタン入力まで少し待つ (連打速度調整)
    ; yas君リクエストにより「超高速反応(4)」に設定！
    MOVLW   d'4'            
    MOVWF   BTN_TIMER
BTN_DONE
    RETURN

;====================================================================
; 【画面描画マネージャー】
; 巨大な絵描きの仕事を、15人の小人に分けてやらせる仕組みです
;====================================================================
DISP_TASK_MANAGER
    CLRF    PCLATH          ; 【重要】ジャンプ先のページズレを直す
    MOVF    DISP_STATE, W
    ADDWF   PCL, F          ; タスク番号の場所へワープ！

    ; 仕事のリスト
    GOTO    STATE_00_PREP   ; [0] 計算準備
    GOTO    STATE_01_D1K_T  ; [1] 1000の位の上半分
    GOTO    STATE_02_D1H_T  ; [2]  100の位の上半分
    GOTO    STATE_03_D1T_T  ; [3]   10の位の上半分
    GOTO    STATE_04_D1_T   ; [4]    1の位の上半分
    GOTO    STATE_05_PREP2  ; [5] 下の段へ移動
    GOTO    STATE_06_D1K_B  ; [6] 1000の位の下半分
    GOTO    STATE_07_D1H_B  ; [7]  100の位の下半分
    GOTO    STATE_08_D1T_B  ; [8]   10の位の下半分
    GOTO    STATE_09_D1_B   ; [9]    1の位の下半分
    GOTO    STATE_10_PREP3  ; [10] バーグラフへ移動
    GOTO    STATE_11_BAR1   ; [11] バーを少し描く
    GOTO    STATE_12_BAR2   ; [12] バーをもっと描く
    GOTO    STATE_13_BAR3   ; [13] バーをさらに描く
    GOTO    STATE_14_BAR4   ; [14] バー完成！

STATE_00_PREP
    CALL    CONVERT_BCD     ; 今の回転数を表示用に変換
    CALL    CALC_BAR_LEN    ; バーの長さを計算
    MOVLW   0xB0            ; カーソルを左上へ
    CALL    OLED_CMD
    MOVLW   0x00
    CALL    OLED_CMD
    MOVLW   0x10
    CALL    OLED_CMD
    INCF    DISP_STATE, F   ; 次の人の番へ
    RETURN

STATE_01_D1K_T
    CALL    START_DATA_MODE ; 「これから絵を送るぞ」
    MOVF    BCD_1000, W     ; 1000の位の数字を持ってくる
    CALL    SEND_BIG_TOP    ; 上半分を描く
    CALL    I2C_STOP        ; 通信終わり
    INCF    DISP_STATE, F
    RETURN
; (以下、同じように各数字を描いていく...)
STATE_02_D1H_T
    CALL    START_DATA_MODE
    MOVF    BCD_100, W
    CALL    SEND_BIG_TOP
    CALL    I2C_STOP
    INCF    DISP_STATE, F
    RETURN
STATE_03_D1T_T
    CALL    START_DATA_MODE
    MOVF    BCD_10, W
    CALL    SEND_BIG_TOP
    CALL    I2C_STOP
    INCF    DISP_STATE, F
    RETURN
STATE_04_D1_T
    CALL    START_DATA_MODE
    MOVF    BCD_1, W
    CALL    SEND_BIG_TOP
    CALL    I2C_STOP
    INCF    DISP_STATE, F
    RETURN
STATE_05_PREP2
    MOVLW   0xB1            ; カーソルを下段へ
    CALL    OLED_CMD
    MOVLW   0x00
    CALL    OLED_CMD
    MOVLW   0x10
    CALL    OLED_CMD
    INCF    DISP_STATE, F
    RETURN
STATE_06_D1K_B
    CALL    START_DATA_MODE
    MOVF    BCD_1000, W
    CALL    SEND_BIG_BTM    ; 下半分を描く
    CALL    I2C_STOP
    INCF    DISP_STATE, F
    RETURN
STATE_07_D1H_B
    CALL    START_DATA_MODE
    MOVF    BCD_100, W
    CALL    SEND_BIG_BTM
    CALL    I2C_STOP
    INCF    DISP_STATE, F
    RETURN
STATE_08_D1T_B
    CALL    START_DATA_MODE
    MOVF    BCD_10, W
    CALL    SEND_BIG_BTM
    CALL    I2C_STOP
    INCF    DISP_STATE, F
    RETURN
STATE_09_D1_B
    CALL    START_DATA_MODE
    MOVF    BCD_1, W
    CALL    SEND_BIG_BTM
    CALL    I2C_STOP
    INCF    DISP_STATE, F
    RETURN
STATE_10_PREP3
    MOVLW   0xB3            ; カーソルをバーグラフの行へ
    CALL    OLED_CMD
    MOVLW   0x00
    CALL    OLED_CMD
    MOVLW   0x10
    CALL    OLED_CMD
    MOVF    BAR_LEN, W
    MOVWF   BAR_TEMP        ; 長さをコピー
    INCF    DISP_STATE, F
    RETURN
STATE_11_BAR1
    CALL    DRAW_BAR_CHUNK  ; バーを32ドット分描く
    INCF    DISP_STATE, F
    RETURN
STATE_12_BAR2
    CALL    DRAW_BAR_CHUNK
    INCF    DISP_STATE, F
    RETURN
STATE_13_BAR3
    CALL    DRAW_BAR_CHUNK
    INCF    DISP_STATE, F
    RETURN
STATE_14_BAR4
    CALL    DRAW_BAR_CHUNK
    CLRF    DISP_STATE      ; 全員終わった！最初(0)に戻る
    RETURN

;====================================================================
; 【縁の下の力持ち (補助プログラム)】
;====================================================================
START_DATA_MODE
    CALL    I2C_START
    MOVLW   0x78
    CALL    I2C_SEND
    MOVLW   0x40
    CALL    I2C_SEND
    RETURN

CALC_BAR_LEN
    ; 回転数を64で割って、バーの長さを決める
    ; (右に6回ずらすと、64で割ったのと同じになる！)
    MOVF    RPM_L, W
    MOVWF   NUM_L
    MOVF    RPM_H, W
    MOVWF   NUM_H
    MOVLW   d'6'
    MOVWF   W_REC_CNT
CALC_S_LP
    BCF     STATUS, C
    RRF     NUM_H, F
    RRF     NUM_L, F
    DECFSZ  W_REC_CNT, F
    GOTO    CALC_S_LP
    MOVF    NUM_L, W
    MOVWF   BAR_LEN
    ; 長すぎたら128cm(ドット)で切る
    MOVF    NUM_H, W
    BTFSS   STATUS, Z
    GOTO    SetMaxBar
    BTFSC   NUM_L, 7
    GOTO    SetMaxBar
    RETURN
SetMaxBar
    MOVLW   d'128'
    MOVWF   BAR_LEN
    RETURN

DRAW_BAR_CHUNK
    CALL    START_DATA_MODE
    MOVLW   d'32'           ; 32回繰り返す
    MOVWF   LOOP_COL
DB_LP
    MOVF    BAR_TEMP, F     ; まだ長さが残ってる？
    BTFSC   STATUS, Z       ; 0なら
    GOTO    DB_SPC          ; 空っぽ(黒)を描く
DB_BAR
    MOVLW   0x7E            ; バー(■)を描く
    CALL    I2C_SEND
    DECF    BAR_TEMP, F     ; 残り長さを1減らす
    GOTO    DB_NXT
DB_SPC
    MOVLW   0x00            ; 黒を描く
    CALL    I2C_SEND
DB_NXT
    DECFSZ  LOOP_COL, F
    GOTO    DB_LP
    CALL    I2C_STOP
    RETURN

CONVERT_BCD
    ; 16進数の回転数を、表示できる10進数に直す
    ; (ひたすら引き算をする古典的な方法)
    MOVF    RPM_L, W
    MOVWF   NUM_L
    MOVF    RPM_H, W
    MOVWF   NUM_H
    CLRF    BCD_1000
    CLRF    BCD_100
    CLRF    BCD_10
C_1K
    MOVLW   low d'1000'
    SUBWF   NUM_L, W
    MOVWF   BCD_1
    MOVLW   high d'1000'
    BTFSS   STATUS, C
    ADDLW   1
    SUBWF   NUM_H, W
    BTFSS   STATUS, C
    GOTO    C_1H
    MOVWF   NUM_H
    MOVF    BCD_1, W
    MOVWF   NUM_L
    INCF    BCD_1000, F
    GOTO    C_1K
C_1H
    MOVLW   d'100'
    SUBWF   NUM_L, W
    MOVWF   BCD_1
    MOVLW   0
    BTFSS   STATUS, C
    ADDLW   1
    SUBWF   NUM_H, W
    BTFSS   STATUS, C
    GOTO    C_1T
    MOVWF   NUM_H
    MOVF    BCD_1, W
    MOVWF   NUM_L
    INCF    BCD_100, F
    GOTO    C_1H
C_1T
    MOVLW   d'10'
    SUBWF   NUM_L, W
    BTFSS   STATUS, C
    GOTO    C_E
    MOVWF   NUM_L
    INCF    BCD_10, F
    GOTO    C_1T
C_E
    MOVF    NUM_L, W
    MOVWF   BCD_1
    RETURN

SEND_BIG_TOP
    ; 文字の上半分を2倍に引き伸ばして送る
    MOVWF   NUM_H
    MOVLW   d'5'
    MOVWF   W_REC_CNT
    MOVF    NUM_H, W
    MOVWF   FONT_PTR_L
    BCF     STATUS, C
    RLF     FONT_PTR_L, F
    RLF     FONT_PTR_L, F
    MOVF    NUM_H, W
    ADDWF   FONT_PTR_L, F
SBT_LP
    MOVF    FONT_PTR_L, W
    CALL    GET_FONT
    ANDLW   0x0F
    CALL    GET_EXPANDED    ; ビットを広げる魔法
    MOVWF   EXP_BUFF
    CALL    I2C_SEND
    MOVF    EXP_BUFF, W
    CALL    I2C_SEND        ; 横にも広げるために2回送る
    INCF    FONT_PTR_L, F
    DECFSZ  W_REC_CNT, F
    GOTO    SBT_LP
    MOVLW   0x00
    CALL    I2C_SEND
    MOVLW   0x00
    CALL    I2C_SEND
    RETURN

SEND_BIG_BTM
    ; 文字の下半分を2倍に引き伸ばして送る
    MOVWF   NUM_H
    MOVLW   d'5'
    MOVWF   W_REC_CNT
    MOVF    NUM_H, W
    MOVWF   FONT_PTR_L
    BCF     STATUS, C
    RLF     FONT_PTR_L, F
    RLF     FONT_PTR_L, F
    MOVF    NUM_H, W
    ADDWF   FONT_PTR_L, F
SBB_LP
    MOVF    FONT_PTR_L, W
    CALL    GET_FONT
    MOVWF   NUM_L
    SWAPF   NUM_L, W        ; 上と下をひっくり返す
    ANDLW   0x0F
    CALL    GET_EXPANDED    ; ビットを広げる魔法
    MOVWF   EXP_BUFF
    CALL    I2C_SEND
    MOVF    EXP_BUFF, W
    CALL    I2C_SEND        ; 2回送る
    INCF    FONT_PTR_L, F
    DECFSZ  W_REC_CNT, F
    GOTO    SBB_LP
    MOVLW   0x00
    CALL    I2C_SEND
    MOVLW   0x00
    CALL    I2C_SEND
    RETURN

;====================================================================
; 【I2C通信・待機・OLED基本操作】
;====================================================================
I2C_BUS_RECOVERY
    MOVLW   d'9'
    MOVWF   W_REC_CNT
BUS_R_LP
    BSF     STATUS, RP0
    BSF     TRISA, 1
    BCF     STATUS, RP0
    CALL    WAIT_1MS
    BSF     STATUS, RP0
    BCF     TRISA, 1
    BCF     STATUS, RP0
    CALL    WAIT_1MS
    DECFSZ  W_REC_CNT, F
    GOTO    BUS_R_LP
    RETURN

I2C_WAIT
    MOVLW   d'10'
    MOVWF   W_I2C
I2C_W_LP
    DECFSZ  W_I2C, F
    GOTO    I2C_W_LP
    RETURN

I2C_START
    BSF     STATUS, RP0
    BSF     TRISA, 0
    BSF     TRISA, 1
    BCF     STATUS, RP0
    CALL    I2C_WAIT
    BSF     STATUS, RP0
    BCF     TRISA, 0
    CALL    I2C_WAIT
    BCF     TRISA, 1
    BCF     STATUS, RP0
    RETURN

I2C_STOP
    BSF     STATUS, RP0
    BCF     TRISA, 0
    CALL    I2C_WAIT
    BSF     TRISA, 1
    CALL    I2C_WAIT
    BSF     STATUS, RP0
    BSF     TRISA, 0
    BCF     STATUS, RP0
    CALL    I2C_WAIT
    RETURN

I2C_SEND
    MOVWF   I2C_DATA
    MOVLW   d'8'
    MOVWF   BIT_CNT
I2C_S_LP
    BCF     STATUS, C
    RLF     I2C_DATA, F
    BSF     STATUS, RP0
    BTFSC   STATUS, C
    BSF     TRISA, 0
    BTFSS   STATUS, C
    BCF     TRISA, 0
    CALL    I2C_WAIT
    BSF     TRISA, 1
    CALL    I2C_WAIT
    BCF     TRISA, 1
    BCF     STATUS, RP0
    DECFSZ  BIT_CNT, F
    GOTO    I2C_S_LP
    BSF     STATUS, RP0
    BSF     TRISA, 0
    BSF     TRISA, 1
    BCF     STATUS, RP0
    CALL    I2C_WAIT
    BSF     STATUS, RP0
    BCF     TRISA, 1
    BCF     STATUS, RP0
    RETURN

OLED_CMD
    MOVWF   NUM_L
    CALL    I2C_START
    MOVLW   0x78
    CALL    I2C_SEND
    MOVLW   0x00
    CALL    I2C_SEND
    MOVF    NUM_L, W
    CALL    I2C_SEND
    CALL    I2C_STOP
    RETURN

OLED_INITIALIZE_FINAL
    ; OLEDを目覚めさせる魔法の呪文たち
    MOVLW   0xAE
    CALL    OLED_CMD
    MOVLW   0xA8
    CALL    OLED_CMD
    MOVLW   0x1F
    CALL    OLED_CMD
    MOVLW   0xD3
    CALL    OLED_CMD
    MOVLW   0x00
    CALL    OLED_CMD
    MOVLW   0x40
    CALL    OLED_CMD
    MOVLW   0x8D
    CALL    OLED_CMD
    MOVLW   0x14
    CALL    OLED_CMD
    MOVLW   0xDA
    CALL    OLED_CMD
    MOVLW   0x02
    CALL    OLED_CMD
    MOVLW   0xA1
    CALL    OLED_CMD
    MOVLW   0xC8
    CALL    OLED_CMD
    MOVLW   0xAF
    CALL    OLED_CMD
    RETURN

OLED_CLEAR_ALL
    ; 画面の隅々まで「黒」を送りつける
    MOVLW   0xB0
    MOVWF   LOOP_PAGE
CL_P_LP
    MOVF    LOOP_PAGE, W
    CALL    OLED_CMD
    MOVLW   0x00
    CALL    OLED_CMD
    MOVLW   0x10
    CALL    OLED_CMD
    CALL    I2C_START
    MOVLW   0x78
    CALL    I2C_SEND
    MOVLW   0x40
    CALL    I2C_SEND
    MOVLW   d'128'
    MOVWF   LOOP_COL
CL_C_LP
    MOVLW   0x00
    CALL    I2C_SEND
    DECFSZ  LOOP_COL, F
    GOTO    CL_C_LP
    CALL    I2C_STOP
    INCF    LOOP_PAGE, F
    MOVLW   0xB4
    SUBWF   LOOP_PAGE, W
    BTFSS   STATUS, Z
    GOTO    CL_P_LP
    RETURN

WAIT_1MS
    MOVLW   d'250'
    MOVWF   W_MS_INNER
W_MS_LP
    NOP
    DECFSZ  W_MS_INNER, F
    GOTO    W_MS_LP
    RETURN

WAIT_50MS
    MOVLW   d'50'
    MOVWF   W_MS_OUTER
W50_LP
    CALL    WAIT_1MS
    DECFSZ  W_MS_OUTER, F
    GOTO    W50_LP
    RETURN

WAIT_500MS
    MOVLW   d'10'
    MOVWF   W_REC_CNT
W500_LP
    CALL    WAIT_50MS
    DECFSZ  W_REC_CNT, F
    GOTO    W500_LP
    RETURN

;====================================================================
; 【データ置き場】 (ここはプログラムではなく、辞書です)
;====================================================================
    ORG     0x300

GET_EXPANDED
    MOVWF   NUM_L
    MOVLW   high EXP_TABLE  ; ページを合わせる！超重要！
    MOVWF   PCLATH
    MOVF    NUM_L, W
    ADDWF   PCL, F
EXP_TABLE
    ; 4ビットのデータを、隙間を空けて8ビットに広げる表
    RETLW   b'00000000'
    RETLW   b'00000011'
    RETLW   b'00001100'
    RETLW   b'00001111'
    RETLW   b'00110000'
    RETLW   b'00110011'
    RETLW   b'00111100'
    RETLW   b'00111111'
    RETLW   b'11000000'
    RETLW   b'11000011'
    RETLW   b'11001100'
    RETLW   b'11001111'
    RETLW   b'11110000'
    RETLW   b'11110011'
    RETLW   b'11111100'
    RETLW   b'11111111'

GET_FONT
    MOVWF   NUM_L
    MOVLW   high FONT_DATA
    MOVWF   PCLATH
    MOVF    NUM_L, W
    ADDWF   PCL, F
FONT_DATA
    ; 数字の形データ (5x8ドット)
    RETLW 0x3E ; 0
    RETLW 0x51
    RETLW 0x49
    RETLW 0x45
    RETLW 0x3E
    RETLW 0x00 ; 1
    RETLW 0x42
    RETLW 0x7F
    RETLW 0x40
    RETLW 0x00
    RETLW 0x42 ; 2
    RETLW 0x61
    RETLW 0x51
    RETLW 0x49
    RETLW 0x46
    RETLW 0x21 ; 3
    RETLW 0x41
    RETLW 0x45
    RETLW 0x4B
    RETLW 0x31
    RETLW 0x18 ; 4
    RETLW 0x14
    RETLW 0x12
    RETLW 0x7F
    RETLW 0x10
    RETLW 0x27 ; 5
    RETLW 0x45
    RETLW 0x45
    RETLW 0x45
    RETLW 0x39
    RETLW 0x3C ; 6
    RETLW 0x4A
    RETLW 0x49
    RETLW 0x49
    RETLW 0x30
    RETLW 0x01 ; 7
    RETLW 0x71
    RETLW 0x09
    RETLW 0x05
    RETLW 0x03
    RETLW 0x36 ; 8
    RETLW 0x49
    RETLW 0x49
    RETLW 0x49
    RETLW 0x36
    RETLW 0x06 ; 9
    RETLW 0x49
    RETLW 0x49
    RETLW 0x29
    RETLW 0x1E

    END