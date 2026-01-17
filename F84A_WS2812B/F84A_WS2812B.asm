;====================================================================
; Title:   PIC16F84A WS2812B Driver [Ultimate Edition]
; Author:  yas & Gemini
; Date:    2026/01/16
; CPU:     PIC16F84A (20MHz Clock is REQUIRED!)
; Note:    1 Cycle = 0.2us (200ns)
;====================================================================
    LIST        P=16F84A
    INCLUDE     "P16F84A.INC"

    ; --- 設定 (重要！) ---
    ; _HS_OSC  : 20MHzの高速クロックを使う宣言
    ; _WDT_OFF : 勝手にリセットされないように番犬をOFF
    __CONFIG    _HS_OSC & _WDT_OFF & _PWRTE_ON & _CP_OFF

;--------------------------------------------------------------------
; 変数定義 (RAMはたったの68バイト！)
;--------------------------------------------------------------------
CBLOCK  0x0C
    ; LEDデータバッファ (10個 x 3色 = 30バイト)
    ; ここに色のデータが入る
    LED_DATA:30 
    
    ; 計算用・ループ用の変数
    cnt_i       ; ループ回数を数えるやつ
    cnt_j       ; ウェイト待ち時間を数えるやつ
    cnt_k       ; 色計算の一時保存用
    
    ; モード管理
    mode        ; 0:消灯, 1:レインボー, 2:ナイトライダー
    
    ; エフェクト用の変数
    hue         ; 七色の色相 (0-255)
    kr_pos      ; ナイトライダーの現在位置 (0-9)
    kr_dir      ; ナイトライダーの動く方向 (0:右, 1:左)
    
    ; 通信用の変数
    send_byte   ; 送信する1バイトデータ
    bit_cnt     ; 8ビット数えるカウンタ
ENDC

; ピンの定義 (配線を変える時はここを直す)
#DEFINE LED_PIN     PORTB, 0  ; LEDへの出力ピン
#DEFINE SW_PIN      PORTA, 0  ; スイッチ入力ピン

;--------------------------------------------------------------------
; プログラム開始地点
;--------------------------------------------------------------------
    ORG     0x000
    GOTO    INIT

;--------------------------------------------------------------------
; 初期化設定 (電源ONで最初にやること)
;--------------------------------------------------------------------
INIT
    ; 入出力の設定 (バンク切り替えという儀式)
    BSF     STATUS, RP0     ; バンク1へ移動
    MOVLW   b'00001'        ; RA0を入力(1)、他を出力(0)に
    MOVWF   TRISA
    MOVLW   b'00000000'     ; RB全て出力に
    MOVWF   TRISB
    BCF     STATUS, RP0     ; バンク0に戻る
    
    ; 中身をキレイにする
    CLRF    PORTA
    CLRF    PORTB
    CLRF    mode
    CLRF    hue
    CLRF    kr_pos
    CLRF    kr_dir
    CALL    CLEAR_BUFFER    ; LEDデータを真っ黒に

;--------------------------------------------------------------------
; メインループ (ここを永遠にぐるぐる回る)
;--------------------------------------------------------------------
MAIN_LOOP
    ; --- スイッチ判定 ---
    BTFSC   SW_PIN          ; スイッチ押された？ (Lowになった？)
    GOTO    NO_SWITCH       ; Highなら押されてない -> スキップ
    
    ; チャタリング(接点のブレ)対策
    CALL    WAIT_10MS
    BTFSC   SW_PIN          ; まだ押されてる？
    GOTO    NO_SWITCH       ; 離してた -> 無視
    
    ; モードを進める (0 -> 1 -> 2 -> 0)
    INCF    mode, F
    MOVLW   3
    SUBWF   mode, W
    BTFSC   STATUS, Z       ; 3になったら
    CLRF    mode            ; 0に戻す
    
    ; モード切替演出 (一度消してリセット感を出す)
    CALL    CLEAR_BUFFER
    CALL    SEND_LEDS
    
    ; スイッチから指が離れるまで待つ (連打防止)
WAIT_REL
    BTFSS   SW_PIN
    GOTO    WAIT_REL
    CALL    WAIT_10MS

NO_SWITCH
    ; --- 現在のモードに応じてジャンプ ---
    MOVF    mode, W
    ADDWF   PCL, F          ; プログラムカウンタを書き換えて分岐
    GOTO    MODE_OFF        ; mode=0 ならここへ
    GOTO    MODE_RAINBOW    ; mode=1 ならここへ
    GOTO    MODE_KNIGHT     ; mode=2 ならここへ

;--------------------------------------------------------------------
; モード0: 消灯 (お休みモード)
;--------------------------------------------------------------------
MODE_OFF
    CALL    CLEAR_BUFFER
    CALL    SEND_LEDS
    CALL    WAIT_100MS
    GOTO    MAIN_LOOP

;--------------------------------------------------------------------
; モード1: ゲーミング・レインボー
; 七色に光りながら色が回っていく
;--------------------------------------------------------------------
MODE_RAINBOW
    MOVLW   LED_DATA
    MOVWF   FSR             ; メモリの指差し確認位置(FSR)を先頭に
    CLRF    cnt_i           ; LED 0番目からスタート
    
R_LOOP
    ; 色の計算 (今のhue + LEDの位置 * 16)
    ; これで「隣のLEDと少し色が違う」状態を作る
    MOVF    cnt_i, W
    MOVWF   cnt_j
    ; x16 (4回左シフト)
    BCF     STATUS, C
    RLF     cnt_j, F
    RLF     cnt_j, F
    RLF     cnt_j, F
    RLF     cnt_j, F
    
    MOVF    hue, W
    ADDWF   cnt_j, W        ; 色を決定
    CALL    HUE2RGB         ; 色データ(Hue)をRGBに変換してメモリへ
    
    ; 次のLEDへ (G, R, B の3バイト分進める)
    INCF    FSR, F
    INCF    FSR, F
    INCF    FSR, F
    
    INCF    cnt_i, F        ; カウンタ+1
    MOVLW   d'10'           ; 10個終わった？
    SUBWF   cnt_i, W
    BTFSS   STATUS, Z
    GOTO    R_LOOP          ; まだなら次へ
    
    CALL    SEND_LEDS       ; 計算結果をLEDに送信！
    INCF    hue, F          ; 全体の色を少しずらす (アニメーション)
    CALL    WAIT_10MS
    GOTO    MAIN_LOOP

;--------------------------------------------------------------------
; モード2: ナイトライダー (残像スペシャル)
; 赤い光が左右に走り、尾を引く
;--------------------------------------------------------------------
MODE_KNIGHT
    ; 1. 全体を少し暗くする (これが残像の正体！)
    CALL    DECAY_LEDS

    ; 2. 現在位置(kr_pos)だけ明るい赤にする
    MOVF    kr_pos, W
    MOVWF   cnt_i
    
    ; 書き込む場所(アドレス)を計算
    MOVLW   LED_DATA
    MOVWF   FSR
    MOVF    cnt_i, W
    ADDWF   cnt_i, F        ; x2
    ADDWF   cnt_i, W        ; x3 (1個あたり3バイトだから)
    ADDWF   FSR, F          ; FSRをその場所に移動
    
    ; 赤色セット (G=0, R=150, B=0)
    MOVLW   d'0'
    MOVWF   INDF            ; G
    INCF    FSR, F
    MOVLW   d'150'          ; R (眩しいので少し抑えめ)
    MOVWF   INDF
    INCF    FSR, F
    MOVLW   d'0'            ; B
    MOVWF   INDF

    ; 3. 送信
    CALL    SEND_LEDS
    
    ; 4. 次の位置を計算
    BTFSS   kr_dir, 0       ; 今どっち向き？
    GOTO    MV_RIGHT        ; 0なら右へ
    GOTO    MV_LEFT         ; 1なら左へ

MV_RIGHT
    INCF    kr_pos, F       ; 位置 +1
    MOVLW   d'9'
    SUBWF   kr_pos, W
    BTFSC   STATUS, Z       ; 右端(9)に着いた？
    BSF     kr_dir, 0       ; 向きを左(1)へ反転
    GOTO    KR_WAIT
MV_LEFT
    DECF    kr_pos, F       ; 位置 -1
    MOVF    kr_pos, W
    BTFSC   STATUS, Z       ; 左端(0)に着いた？
    BCF     kr_dir, 0       ; 向きを右(0)へ反転

KR_WAIT
    CALL    WAIT_50MS       ; 移動速度調整
    GOTO    MAIN_LOOP


;====================================================================
; ここから先は「魔術」の領域 (サブルーチン)
;====================================================================

;--------------------------------------------------------------------
; DECAY_LEDS (フェードアウト処理)
; すべてのLEDの明るさを半分にする
; 「割り算」は重いので「右ビットシフト」を使う天才的発想
;--------------------------------------------------------------------
DECAY_LEDS
    MOVLW   LED_DATA
    MOVWF   FSR
    MOVLW   d'30'           ; 30バイト全部やる
    MOVWF   cnt_j
DECAY_LP
    BCF     STATUS, C       ; キャリークリア
    RRF     INDF, F         ; 右にズラす = 値が半分になる！
    
    ; ゴミ掃除 (値が5以下になったら完全に消す)
    ; これをやらないと薄ーく光り続けてしまう
    MOVF    INDF, W
    SUBLW   d'5'
    BTFSC   STATUS, C       ; 5以下ならCが立つ
    CLRF    INDF            ; 0にする
    
    INCF    FSR, F
    DECFSZ  cnt_j, F
    GOTO    DECAY_LP
    RETURN

;--------------------------------------------------------------------
; SEND_LEDS (最重要・タイミング生成)
; 20MHz (1命令=200ns) を利用して、パルス波形を直接作る
; WS2812Bの仕様: '0'=400ns High, '1'=800ns High
;--------------------------------------------------------------------
SEND_LEDS
    MOVLW   LED_DATA
    MOVWF   FSR
    MOVLW   d'30'
    MOVWF   cnt_i
BYTE_LP
    MOVF    INDF, W
    MOVWF   send_byte
    MOVLW   d'8'
    MOVWF   bit_cnt
BIT_LP
    ; --- ここからタイミング命 (割り込み厳禁) ---
    RLF     send_byte, F    ; [Cycle 1] 送るビットをCフラグへ押し出す
    
    BSF     LED_PIN         ; [Cycle 2] ONにする (ここから計測開始！)
                            ; 経過時間: 0ns
    
    BTFSS   STATUS, C       ; [Cycle 3] ビットは1か？
                            ; 0なら: 次の命令へ (1サイクル消費)
                            ; 1なら: 次をスキップ (2サイクル消費) -> 時間稼ぎになる！
    
    BCF     LED_PIN         ; [Cycle 4] 0ならここでOFF
                            ; これでON時間は 2サイクル = 400ns ('0'の信号完成)
    
    NOP                     ; [Cycle 5] 時間調整
    
    BCF     LED_PIN         ; [Cycle 6] 1ならここでOFF
                            ; これでON時間は 4サイクル = 800ns ('1'の信号完成)
    ; ---------------------------------------------
    
    DECFSZ  bit_cnt, F
    GOTO    BIT_LOOP_FIX    ; ジャンプ補正(下記参照)
    GOTO    NEXT_BYTE
    
BIT_LOOP_FIX
    GOTO    BIT_LP          ; ループの戻り

NEXT_BYTE
    INCF    FSR, F
    DECFSZ  cnt_i, F
    GOTO    BYTE_LP
    
    ; リセット信号 (50us以上Low)
    MOVLW   d'50'
    MOVWF   cnt_j
RST_WAIT
    NOP
    DECFSZ  cnt_j, F
    GOTO    RST_WAIT
    RETURN

;--------------------------------------------------------------------
; バッファクリア (全消去)
;--------------------------------------------------------------------
CLEAR_BUFFER
    MOVLW   LED_DATA
    MOVWF   FSR
    MOVLW   d'30'
    MOVWF   cnt_j
CL_LP
    CLRF    INDF
    INCF    FSR, F
    DECFSZ  cnt_j, F
    GOTO    CL_LP
    RETURN

;--------------------------------------------------------------------
; 色相(Hue) -> RGB変換 (簡易版)
; 本来は複雑な計算が必要だが、条件分岐でエリア分けして軽量化
;--------------------------------------------------------------------
HUE2RGB
    MOVWF   cnt_k
    ; 面倒な計算は省略！色相を3つのエリアに分けて処理
    MOVLW   d'85'
    SUBWF   cnt_k, W
    BTFSC   STATUS, C
    GOTO    REG_23
REG_1 ; 赤 -> 緑 エリア
    MOVF    cnt_k, W
    ADDWF   cnt_k, W
    MOVWF   INDF            ; G (増える)
    INCF    FSR, F
    COMF    cnt_k, W
    MOVWF   INDF            ; R (減る)
    INCF    FSR, F
    CLRF    INDF            ; B (なし)
    DECF    FSR, F
    DECF    FSR, F
    RETURN
REG_23
    MOVLW   d'170'
    SUBWF   cnt_k, W
    BTFSC   STATUS, C
    GOTO    REG_3
REG_2 ; 緑 -> 青 エリア
    MOVF    cnt_k, W
    MOVWF   cnt_j
    MOVLW   d'85'
    SUBWF   cnt_j, F
    COMF    cnt_j, W
    MOVWF   INDF            ; G (減る)
    INCF    FSR, F
    CLRF    INDF            ; R (なし)
    INCF    FSR, F
    MOVF    cnt_j, W
    ADDWF   cnt_j, W
    MOVWF   INDF            ; B (増える)
    DECF    FSR, F
    DECF    FSR, F
    RETURN
REG_3 ; 青 -> 赤 エリア
    MOVF    cnt_k, W
    MOVWF   cnt_j
    MOVLW   d'170'
    SUBWF   cnt_j, F
    CLRF    INDF            ; G (なし)
    INCF    FSR, F
    MOVF    cnt_j, W
    ADDWF   cnt_j, W
    MOVWF   INDF            ; R (増える)
    INCF    FSR, F
    COMF    cnt_j, W
    MOVWF   INDF            ; B (減る)
    DECF    FSR, F
    DECF    FSR, F
    RETURN

;--------------------------------------------------------------------
; ウェイトルーチン (時間稼ぎ)
; プロセッサを空回りさせて時間を潰す
;--------------------------------------------------------------------
WAIT_100MS
    MOVLW   d'10'
    MOVWF   cnt_k
W_LOOP
    CALL    WAIT_10MS
    DECFSZ  cnt_k, F
    GOTO    W_LOOP
    RETURN

WAIT_50MS
    MOVLW   d'5'
    MOVWF   cnt_k
    GOTO    W_LOOP

WAIT_10MS
    MOVLW   d'200'
    MOVWF   cnt_j
W10_L1
    MOVLW   d'165'
    MOVWF   cnt_i
W10_L2
    DECFSZ  cnt_i, F
    GOTO    W10_L2
    DECFSZ  cnt_j, F
    GOTO    W10_L1
    RETURN

    END