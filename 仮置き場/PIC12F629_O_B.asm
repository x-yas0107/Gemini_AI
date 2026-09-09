;================================================================================
; Version: 0.04
; Change History:
; - 0.01: PIC12F675からPIC12F629へ機種変更（list, #include変更、ANSEL削除）、3回リピート再生機能追加、10秒間のウェイトルーチン追加
; - 0.02: ウェイトルーチンの待機時間を約10秒から約5秒へ短縮（WAIT3の初期値をD'31'からD'15'へ変更）
; - 0.03: 待機中のコイル焼損防止のためCLRF GPIOを各待機前に追加、ループ後の省電力スリープモード追加（IOC割込による復帰設定を含む）
; - 0.04: 再生中のフリーズバグ修正（初期化ルーチンでの常時GPIE許可を廃止し、スリープ直前・直後のみでGPIEを切り替えるよう安全対策を実装）
;================================================================================

;	PIC12F629	FAMILY MART チャイム  

;            _____________
;    VDD  =1;            ;8= 0V VSS
;    SP   =2;(GP5)  (GP0);7= SP
;    SP   =3;(GP4)  (GP1);6= SP
;   BP	 =4;(GP3)  (GP2);5= SP              
;            _____________                
;  

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

	list      p=12f629	; 翻訳時にリストファイルを作ります
	#include <p12f629.inc>	; 12F629用定義ファイルを読み込みます

	;ASMファイルでは「;」より右側は読み取りませんので注意書きなり落書きが可能です。

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

	__CONFIG _CP_OFF & _CPD_OFF & _WDT_OFF & _MCLRE_OFF & _PWRTE_ON & _INTRC_OSC_NOCLKOUT	;4MHz	;_HS_OSC
									
	;↑↑動かす条件です。機種によっていろいろ取り決めはありますが、これらは理屈抜きに覚えてください。		

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

	errorlevel  -302	; 翻訳時に302エラーが出ないようにします。													

	ORG	000H		;初期化ルーチン
	GOTO	INIT 

	ORG	004H		;割り込みルーチン
	GOTO	INT




	COUNT		EQU	20H	;「COUNT」という箱の住所  20H
	PIYO		EQU	21H
	PIYO1		EQU	22H
	PIYO2		EQU	23H
	PIYO3		EQU	24H
	REP_CNT		EQU	25H
	WAIT1		EQU	26H
	WAIT2		EQU	27H
	WAIT3		EQU	28H

	DD		EQU	40H
	STATUS_BU	EQU	41H
	WARI		EQU	42H
	OVFL_C		EQU	43H


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;＜＜＜初期化ルーチン＞＞＞
INIT:

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;プログラムは「GOTO」などの命令がない限り下へ下へと降りて行きます。
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

	BSF	STATUS,RP0			;BANK1に切り替え	STATUS,RP0を１にするという意味

	MOVLW   B'00001000'			;OUT=GP0, OUT=GP1, OUT=GP2, IN=GP3, OUT=GP4, OUT=GP5
	MOVWF	TRISIO                      	;GPIO

	MOVLW	B'00001000'			;GP3の状態変化割込(Wake-up用)を設定
	MOVWF	IOC

	MOVLW   B'10000111'  			;割り込みはTMR0 1:256			             
	MOVWF	OPTION_REG 			

;	MOVLW	B'00000000'			;GP3, GP2, GP1, GP0 (12F629では不要のため無効化)
;	MOVWF	ANSEL				;(12F629では不要のため無効化)

	BCF	STATUS,RP0			;BANK0に切り替え	STATUS,RP0を０にするという意味

	BSF	INTCON,T0IE			;タイマ割込許可
	BSF	INTCON,GIE			;全体割込許可

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;＜＜＜音階＞＞＞

	DO	EQU	D'120'
	DOS	EQU	D'113'
	RE	EQU	D'107'
	RE_S	EQU	D'101'
	MI	EQU	D'95'
	FA	EQU	D'90'
	FAS	EQU	D'85'
	SO	EQU	D'80'
	SOS	EQU	D'76'
	RA	EQU	D'71'
	RAS	EQU	D'67'
	SI	EQU	D'64'
	DOH	EQU	D'60'
	DOSH	EQU	D'57'
	REH	EQU	D'53'
	RESH	EQU	D'50'
	MIH	EQU	D'48'
	FAH	EQU	D'45'
	FASH	EQU	D'43'
	SOH	EQU	D'40'
	SOSH	EQU	D'38'
	RAH	EQU	D'36'

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

	CLRF	GPIO
	
	CLRF	COUNT
	CLRF	PIYO
	CLRF	PIYO1
	CLRF	PIYO2
	CLRF	PIYO3
	CLRF	REP_CNT
	CLRF	DD
	CLRF	STATUS_BU
	CLRF	WARI


	GOTO	MAIN1

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;＜＜＜割り込みル－チン＞＞＞


INT:
	MOVWF	WARI
	SWAPF	STATUS,W
	MOVWF	STATUS_BU
						
	BCF	INTCON,T0IF
	MOVLW	D'1'				;256-D' '=		
	MOVWF	TMR0
	DECFSZ	OVFL_C,F

	GOTO	INT_1
	MOVLW	D'7'				;＜＜＜＜テンポ☆
	MOVWF	OVFL_C	
		
	BSF	DD,0			

INT_1
	SWAPF	STATUS_BU,W
	MOVWF	STATUS
	MOVF	WARI,W

	RETFIE 

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;＜＜＜＜BGMテーブル＞＞＞＞

TABLE_BGM
	
	MOVF	COUNT,W
	ADDWF	PCL,F

	RETLW	RA		;1
	RETLW	FA		;2
	RETLW	DO		;3
	RETLW	FA		;4

	RETLW	SO		;5
	RETLW	DOH		;6
	RETLW	DOH		;7
	RETLW	DO		;8

	RETLW	SO		;9
	RETLW	RA		;10
	RETLW	SO		;11
	RETLW	DO		;12

	RETLW	FA		;13
	RETLW	FA		;14
	RETLW	FA		;15

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

MAIN1
	CLRF	GPIO		;スピーカー確実OFF（コイル保護）
	BCF	INTCON,GIE	;スリープ中の割込ジャンプ禁止
	BSF	INTCON,GPIE	;スリープ直前にピン状態変化割込許可
	MOVF	GPIO,W		;ピン状態読み込み（変化リセット）
	BCF	INTCON,GPIF	;状態変化フラグクリア
	SLEEP			;スリープ（ボタンが離されるのを待つ）
	NOP
	BCF	INTCON,GPIE	;復帰直後にピン状態変化割込禁止（フリーズ防止）
	MOVF	GPIO,W		;復帰後、再度読み込み
	BCF	INTCON,GPIF	;割込フラグを確実にクリア
	BSF	INTCON,GIE	;割込許可再開

	BTFSS	GPIO,3		;PB OFF？
	GOTO	MAIN1		;NO		
				;YES

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

MAIN2
	CLRF	GPIO		;スピーカー確実OFF（コイル保護）
	BCF	INTCON,GIE	;スリープ中の割込ジャンプ禁止
	BSF	INTCON,GPIE	;スリープ直前にピン状態変化割込許可
	MOVF	GPIO,W		;ピン状態読み込み（変化リセット）
	BCF	INTCON,GPIF	;状態変化フラグクリア
	SLEEP			;スリープ（省電力待機・ボタン待ち）
	NOP
	BCF	INTCON,GPIE	;復帰直後にピン状態変化割込禁止（フリーズ防止）
	MOVF	GPIO,W		;復帰後、再度読み込み
	BCF	INTCON,GPIF	;割込フラグを確実にクリア
	BSF	INTCON,GIE	;割込許可再開

	BTFSC	GPIO,3		;PB ON？
	GOTO	MAIN2		;NO
				;YES
	
	MOVLW	D'3'		;再生回数3回セット
	MOVWF	REP_CNT

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

MODE

	MOVLW	D'1'				;256-D' '=
	MOVWF	TMR0
	MOVLW	D'7'				;＜＜＜＜テンポ☆
	MOVWF	OVFL_C

MAIN_A
	BCF	DD,0

	CALL	TABLE_BGM	;BGMテーブル
	MOVWF	PIYO		;音階代入A B C

	INCF	COUNT,F	;カウンターテーブルカウント値
	MOVLW	D'15'		;＜＜＜＜＜＜＜66
	SUBWF	COUNT,W	;カウンターテーブルカウント値
	BTFSS	STATUS,C
	GOTO	MAIN_B

	CLRF	COUNT
	DECFSZ	REP_CNT,F	;再生回数を減らして0ならMAIN1(スリープ)へ
	GOTO	WAIT_5S
	GOTO	MAIN1

WAIT_5S
	CLRF	GPIO		;待機中のスピーカー確実OFF（コイル保護）
	MOVLW	D'15'		;約5秒の待機ループ
	MOVWF	WAIT3
WAIT_LP3
	MOVLW	D'255'
	MOVWF	WAIT2
WAIT_LP2
	MOVLW	D'255'
	MOVWF	WAIT1
WAIT_LP1
	NOP
	NOP
	DECFSZ	WAIT1,F
	GOTO	WAIT_LP1
	DECFSZ	WAIT2,F
	GOTO	WAIT_LP2
	DECFSZ	WAIT3,F
	GOTO	WAIT_LP3
	
	GOTO	MODE		;待機終了後、再生へ戻る

MAIN_B
	MOVF	PIYO,W
	SUBWF	PIYO3
	BTFSC	STATUS,Z		
	GOTO	T_C3

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

T_A1				;＜＜＜波形Ａ＞＞＞	
	MOVLW	D'100'		
	MOVWF	PIYO2

T_A2
	MOVF	PIYO,W
	MOVWF	PIYO1
	MOVWF	PIYO3

A_1LP
	CLRF	GPIO		;音OFF
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	DECFSZ	PIYO1,F
	GOTO	A_1LP

	MOVF	PIYO,W
	MOVWF	PIYO1
A_2LP
	MOVLW	B'11111111'		;音ON
	MOVWF	GPIO
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	DECFSZ	PIYO1,F
	GOTO	A_2LP

	BTFSC	DD,0
	GOTO	MAIN_A

	DECFSZ	PIYO2,F
	GOTO	T_A2

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;				

T_B1				;＜＜＜波形Ｂ＞＞＞	
	MOVLW	D'100'			
	MOVWF	PIYO2
T_B2
	MOVF	PIYO,W
	MOVWF	PIYO1
	MOVWF	PIYO3


B_1LP
	CLRF	GPIO		;音OFF
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	DECFSZ	PIYO1,F
	GOTO	B_1LP

	MOVF	PIYO,W
	MOVWF	PIYO1
B_2LP
	MOVLW	B'11111111'		;音ON
	MOVWF	GPIO
	NOP
	NOP
	NOP
	NOP
	CLRF	GPIO		;音OFF
	NOP
	DECFSZ	PIYO1,F
	GOTO	B_2LP

	BTFSC	DD,0
	GOTO	MAIN_A

	DECFSZ	PIYO2,F
	GOTO	T_B2

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

T_C1				;＜＜波形Ｃ＞＞＞	
	MOVLW	D'255'			
	MOVWF	PIYO2

T_C2
	MOVF	PIYO,W
	MOVWF	PIYO1
	MOVWF	PIYO3

C_1LP
	CLRF	GPIO		;音OFF
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	DECFSZ	PIYO1,F
	GOTO	C_1LP

	MOVF	PIYO,W
	MOVWF	PIYO1
C_2LP
	MOVLW	B'11111111'		;音ON
	MOVWF	GPIO
	NOP
	NOP
	NOP
	CLRF	GPIO		;音OFF
	NOP
	NOP
	DECFSZ	PIYO1,F
	GOTO	C_2LP

	BTFSC	DD,0
	GOTO	MAIN_A

T_C3			
	DECFSZ	PIYO2,F
	GOTO	T_C2

	GOTO	T_C1

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

	END