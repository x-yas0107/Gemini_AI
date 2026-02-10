;==============================================================================
;   PROJECT: TRUE DIVERGENCE CLOCK (ダイバージェンス・メーター風時計)
;   VERSION: Ver 1.00 (Final Release)
;   DATE   : 2026/02/10
;   AUTHORS: yas & Gemini
;   TARGET : PIC16F84A
;   OSC    : 32.768kHz (水晶発振)
;
;   [ 概要 ]
;   ニキシー管の「世界線変動率」演出をLEDで再現した高精度時計。
;   ボタンを押すと数値がシャッフルされ、現在の時刻を表示します。
;   表示中に時刻が進んでも、数字が崩れない「ラッチ機能」搭載。
;
;   [ ピン配置 / Pin Assignment ]
;                   _____________
;   (RA2) BUTTON --|1          18|-- (RA1) GREEN LED (カソード制御)
;   (RA3) ------ --|2          17|-- (RA0) RED LED   (カソード制御)
;   (RA4) ------ --|3          16|-- (OSC1) 32.768kHz Crystal
;   (MCLR)RESET  --|4          15|-- (OSC2) 32.768kHz Crystal
;   (Vss) GND    --|5          14|-- (Vdd) +3V ~ +5V
;   (RB0) SEG A  --|6          13|-- (RB7) SEG DP / Point
;   (RB1) SEG B  --|7          12|-- (RB6) SEG G
;   (RB2) SEG C  --|8          11|-- (RB5) SEG F
;   (RB3) SEG D  --|9          10|-- (RB4) SEG E
;                   -------------
;==============================================================================
    LIST      P=16F84A
    INCLUDE   <P16F84A.INC>
    ; 設定: 低速水晶(LP), 番犬OFF, 電源待ちON, コード保護OFF
    __CONFIG  _LP_OSC & _WDT_OFF & _PWRTE_ON & _CP_OFF

    ; --- メモ帳（変数）の定義 ---
    CBLOCK  0x0C
    SEC, MIN, HOUR       ; 現在の時刻（秒・分・時）
    MODE, PHASE          ; 時計のモード、表示の段階
    TICKS                ; 1秒より細かいカウント用 (1/8秒)
    SHUFFLE_CNT, DISP_CNT ; シャッフル回数、表示時間用
    W_BLANK_H, W_BLANK_L ; 待ち時間（空ループ）用
    W_PWM, TEMP          ; 一時的な計算用
    TABLE_IDX            ; 数字の形を決める番号
    CURRENT_VAL          ; 今から表示しようとしている値
    W_PWM_TEST           ; 起動テスト用
    DISP_LATCH           ; ★重要：表示中に数字を固定するためのメモ
    ENDC

    ; --- ピンの名前付け ---
#DEFINE BTN_MAIN PORTA, 2  ; ボタンはRA2
#DEFINE RED_K    PORTA, 0  ; 赤LED制御はRA0
#DEFINE GRN_K    PORTA, 1  ; 緑LED制御はRA1
#DEFINE DP_BIT   B'10000000' ; ドット(DP)の位置

    ORG     0x000
    GOTO    INIT         ; 電源ONでここへ飛ぶ

    ORG     0x004
    GOTO    INIT         ; もしもの時もここへ

;==============================================================================
;   [ 時計の心臓部 ] CLOCK_CHECK
;   ここを常に呼び出すことで、正確に時を刻み続ける。
;   ボタンを押していようが、表示中だろうが、タイマーは止まらない。
;==============================================================================
CLOCK_CHECK
    BTFSS   INTCON, T0IF   ; タイマー(TMR0)が溢れたかチェック
    RETURN                 ; まだなら何もしないで戻る
    
    ; --- 0.125秒が経過した！ ---
    BCF     INTCON, T0IF   ; フラグを掃除して次を待つ
    INCF    TICKS, F       ; 細かい時間を+1
    MOVLW   D'8'
    SUBWF   TICKS, W
    BTFSS   STATUS, Z      ; 8回数えたか？ (0.125秒 x 8 = 1秒)
    RETURN                 ; まだ1秒経ってない
    
    ; --- 1秒が経過した！ ---
    CLRF    TICKS          ; 細かい時間をリセット
    INCF    SEC, F         ; 秒を+1
    MOVLW   D'60'
    SUBWF   SEC, W
    BTFSS   STATUS, Z      ; 60秒になったか？
    RETURN
    CLRF    SEC            ; 0秒に戻す
    INCF    MIN, F         ; 分を+1
    MOVLW   D'60'
    SUBWF   MIN, W
    BTFSS   STATUS, Z      ; 60分になったか？
    RETURN
    CLRF    MIN            ; 0分に戻す
    INCF    HOUR, F        ; 時を+1
    MOVLW   D'24'
    SUBWF   HOUR, W
    BTFSS   STATUS, Z      ; 24時になったか？
    RETURN
    CLRF    HOUR           ; 0時に戻す
    RETURN

;==============================================================================
;   [ 初期設定 ] INIT
;   電源を入れた直後の準備運動
;==============================================================================
INIT
    CLRF    PORTA
    CLRF    PORTB
    BSF     STATUS, RP0    ; バンク切り替え（設定ページへ）
    ERRORLEVEL -302
    MOVLW   B'11111100'    ; RA0,1は出力、RA2～は入力
    MOVWF   TRISA
    CLRF    TRISB          ; RBは全部出力（7セグ用）
    MOVLW   B'10000001'    ; タイマー設定 (プリスケーラ 1:4)
    MOVWF   OPTION_REG
    ERRORLEVEL +302
    BCF     STATUS, RP0    ; バンク戻る
    CLRF    INTCON

    MOVLW   DP_BIT
    MOVWF   PORTB
    
    ; --- 起動デモ：生存確認テスト ---
    ; 1. 赤色点灯 (0.5秒)
    BSF     RED_K
    BCF     GRN_K
    MOVLW   D'15'
    MOVWF   W_BLANK_H
W_T1
    MOVLW   D'100'
    MOVWF   W_BLANK_L
W_T2
    DECFSZ  W_BLANK_L, F
    GOTO    W_T2
    DECFSZ  W_BLANK_H, F
    GOTO    W_T1

    ; 2. オレンジ色点灯 (0.5秒 / 高速切替)
    MOVLW   D'120'
    MOVWF   W_PWM_TEST
W_T_ORANGE
    BSF     RED_K          ; 赤ON
    BCF     GRN_K
    MOVLW   D'5'
    MOVWF   W_BLANK_L
W_T_O1
    DECFSZ  W_BLANK_L, F
    GOTO    W_T_O1
    BCF     RED_K          ; 緑ON
    BSF     GRN_K
    MOVLW   D'5'
    MOVWF   W_BLANK_L
W_T_O2
    DECFSZ  W_BLANK_L, F
    GOTO    W_T_O2
    DECFSZ  W_PWM_TEST, F
    GOTO    W_T_ORANGE

    ; 3. 緑色点灯 (0.5秒)
    BCF     RED_K
    BSF     GRN_K
    MOVLW   D'15'
    MOVWF   W_BLANK_H
W_T5
    MOVLW   D'100'
    MOVWF   W_BLANK_L
W_T6
    DECFSZ  W_BLANK_L, F
    GOTO    W_T6
    DECFSZ  W_BLANK_H, F
    GOTO    W_T5

    BCF     GRN_K          ; 消灯
    CLRF    PORTB
    CLRF    PORTA

    ; --- 時計の開始時刻設定 (00:00:00) ---
    CLRF    HOUR
    CLRF    MIN
    CLRF    SEC
    CLRF    TICKS
    CLRF    MODE
    GOTO    MAIN

;==============================================================================
;   [ メインループ ] MAIN
;   ここをぐるぐる回りながら、モード（待機、演出、表示）を切り替える
;==============================================================================
MAIN
    CALL    CLOCK_CHECK    ; 常に時間を気にする
    MOVF    MODE, W
    BTFSC   STATUS, Z
    GOTO    ST_WAIT        ; モード0なら「ボタン待ち」
    XORLW   D'1'
    BTFSC   STATUS, Z
    GOTO    ST_SHUFFLE     ; モード1なら「シャッフル演出」
    XORLW   D'3'
    BTFSC   STATUS, Z
    GOTO    ST_DIGIT10     ; モード3なら「10の位を表示」
    GOTO    ST_DIGIT01     ; それ以外なら「1の位を表示」

; --- 待機モード：何も表示せずボタンを待つ ---
ST_WAIT
    CALL    CLOCK_CHECK    ; 待ってる間も時計は進める
    CLRF    PORTA
    CLRF    PORTB
    BTFSS   BTN_MAIN       ; ボタン押された？
    GOTO    START_SEQ      ; 押されたら開始！
    GOTO    MAIN

; --- 演出開始の準備 ---
START_SEQ
    CLRF    PHASE          ; 表示フェーズをリセット
    GOTO    RESET_SHUFFLE

; --- シャッフルモード：数字をパラパラ変える ---
ST_SHUFFLE
    CALL    CLOCK_CHECK    ; 演出中も時計は進める
    INCF    TEMP, F        ; 適当な数字を作る
    MOVF    TEMP, W
    ANDLW   0x07
    MOVWF   TABLE_IDX
    CALL    GET_SEG_VAL    ; 数字の形を取得
    MOVWF   PORTB          ; LEDに出す
    CALL    COLOR_CTRL     ; 色を決める
    DECFSZ  SHUFFLE_CNT, F ; シャッフル回数減らす
    GOTO    MAIN
    
    ; ★ここが重要！シャッフルが終わった瞬間の時間をメモする
    CALL    GET_CURRENT_VAL 
    MOVF    CURRENT_VAL, W
    MOVWF   DISP_LATCH     ; メモ帳(DISP_LATCH)に書き写す！
    
    CALL    WAIT_BLANK     ; 少し間を置く
    MOVLW   D'2'           ; 次は「表示」モードへ
    MOVWF   MODE
    MOVLW   D'40'          ; 表示時間は約0.5秒
    MOVWF   DISP_CNT
    GOTO    MAIN

; --- 10の位を表示するモード ---
ST_DIGIT10
    CALL    CLOCK_CHECK    ; 時計は進める
    MOVF    DISP_LATCH, W  ; ★さっきメモした数字を使う（動かない！）
    MOVWF   TEMP
    CLRF    TABLE_IDX
BCD_10                     ; 割り算をして10の位を出す
    MOVLW   D'10'
    SUBWF   TEMP, F
    BTFSS   STATUS, C
    GOTO    DISP_10
    INCF    TABLE_IDX, F
    GOTO    BCD_10
DISP_10
    CALL    GET_SEG_VAL
    MOVWF   PORTB
    CALL    COLOR_CTRL
    DECFSZ  DISP_CNT, F    ; 表示時間を減らす
    GOTO    MAIN
    CALL    WAIT_BLANK     ; 消灯
    MOVLW   D'3'           ; 次は「1の位」モードへ
    MOVWF   MODE
    MOVLW   D'40'
    MOVWF   DISP_CNT
    GOTO    MAIN

; --- 1の位を表示するモード ---
ST_DIGIT01
    CALL    CLOCK_CHECK
    MOVF    DISP_LATCH, W  ; ★ここもメモした数字を使う
    MOVWF   TEMP
BCD_1                      ; 10を引きまくって余り(1の位)を出す
    MOVLW   D'10'
    SUBWF   TEMP, F
    BTFSC   STATUS, C
    GOTO    BCD_1
    MOVLW   D'10'
    ADDWF   TEMP, W
    MOVWF   TABLE_IDX
    CALL    GET_SEG_VAL
    MOVWF   PORTB
    CALL    COLOR_CTRL
    DECFSZ  DISP_CNT, F
    GOTO    MAIN
    CALL    WAIT_BLANK
    INCF    PHASE, F       ; 次の桁（時→分→秒）へ進む準備
    MOVLW   D'3'
    SUBWF   PHASE, W
    BTFSC   STATUS, Z
    GOTO    SEQ_FINISH     ; 全部終わったら最初に戻る
RESET_SHUFFLE
    MOVLW   D'1'           ; シャッフルモードに戻る
    MOVWF   MODE
    MOVLW   D'100'         ; シャッフル回数セット
    MOVWF   SHUFFLE_CNT
    GOTO    MAIN

SEQ_FINISH
    CLRF    MODE           ; 待機モードへ
    GOTO    MAIN

;==============================================================================
;   [ サブルーチン ] いろいろな便利機能
;==============================================================================

; --- 今の時間を取得する ---
GET_CURRENT_VAL
    MOVF    PHASE, W       ; 今は何を表示する番？
    BTFSC   STATUS, Z
    GOTO    IS_RED         ; 0なら「時(赤)」
    ADDLW   -1
    BTFSC   STATUS, Z
    GOTO    IS_ORN         ; 1なら「分(橙)」
    MOVF    SEC, W         ; 2なら「秒」を返す
    MOVWF   CURRENT_VAL
    RETURN
IS_RED
    MOVF    HOUR, W        ; 「時」を返す
    MOVWF   CURRENT_VAL
    RETURN
IS_ORN
    MOVF    MIN, W         ; 「分」を返す
    MOVWF   CURRENT_VAL
    RETURN

; --- 色を制御する ---
COLOR_CTRL
    MOVF    PHASE, W
    BTFSC   STATUS, Z
    GOTO    COLOR_RED      ; 赤
    ADDLW   -1
    BTFSC   STATUS, Z
    GOTO    COLOR_ORN      ; 橙
    GOTO    COLOR_GRN      ; 緑

COLOR_RED
    BSF     RED_K          ; 赤ON、緑OFF
    BCF     GRN_K
    CALL    SHORT_WAIT
    RETURN
COLOR_GRN
    BCF     RED_K          ; 赤OFF、緑ON
    BSF     GRN_K
    CALL    SHORT_WAIT
    RETURN
COLOR_ORN
    BSF     RED_K          ; 赤ON
    BCF     GRN_K
    CALL    SHORT_WAIT
    BCF     RED_K          ; 緑ON（高速切り替えで橙に見せる）
    BSF     GRN_K
    CALL    SHORT_WAIT
    BCF     GRN_K
    RETURN

; --- 少し長い待ち時間（ここでも時計監視！） ---
WAIT_BLANK
    CLRF    PORTA          ; いったん消灯
    MOVLW   D'5'
    MOVWF   W_BLANK_H
W_B_LP
    CALL    CLOCK_CHECK    ; ★待ってる間も時計はずらさない
    MOVLW   D'30'
    MOVWF   W_BLANK_L
W_B_INNER
    DECFSZ  W_BLANK_L, F
    GOTO    W_B_INNER
    DECFSZ  W_BLANK_H, F
    GOTO    W_B_LP
    RETURN

; --- 一瞬のウェイト（PWM用） ---
SHORT_WAIT
    MOVLW   D'15'
    MOVWF   W_PWM
S_W_LP
    DECFSZ  W_PWM, F
    GOTO    S_W_LP
    RETURN

; --- 数字の形データ (7セグメント) ---
GET_SEG_VAL
    MOVF    TABLE_IDX, W
    ANDLW   0x0F
    ADDWF   PCL, F
    RETLW   B'00111111' ;0
    RETLW   B'00000110' ;1
    RETLW   B'01011011' ;2
    RETLW   B'01001111' ;3
    RETLW   B'01100110' ;4
    RETLW   B'01101101' ;5
    RETLW   B'01111101' ;6
    RETLW   B'00000111' ;7
    RETLW   B'01111111' ;8
    RETLW   B'01101111' ;9

    END