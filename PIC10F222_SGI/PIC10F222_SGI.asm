; ==============================================================================
; Project: PIC10F222 Melody Player (Kaeru no Uta)
; Target:  PIC10F222
; Clock:   Internal 4MHz (1us instruction cycle)
; Output:  GP0 (Piezo Buzzer / Speaker)
;
; Version History:
; v0.01 - 2026/09/23 - Initial release. Implemented "Kaeru no Uta" infinite 
;                      loop playback upon power-on using TMR0 for duration.
; ==============================================================================

        list p=10f222
        #include <p10f222.inc>

        __CONFIG _IOSCFS_4MHZ & _MCPU_OFF & _WDT_OFF & _CP_OFF & _MCLRE_OFF

; ------------------------------------------------------------------------------
; Macros & Definitions
; ------------------------------------------------------------------------------
#define N_C 1
#define N_D 2
#define N_E 3
#define N_F 4
#define N_G 5
#define N_A 6
#define N_R 0
#define LNG 0x80

; ------------------------------------------------------------------------------
; Variables
; ------------------------------------------------------------------------------
        CBLOCK 0x09
            SongIndex
            NoteVal
            DelayCount
            DurationCount
            TMR0_Last
            Temp
            Temp2
        ENDC

; ------------------------------------------------------------------------------
; Reset Vector & Initialization
; ------------------------------------------------------------------------------
        ORG 0x0000
        movwf   OSCCAL          ; Load factory calibration

        clrf    ADCON0          ; Disable ADC, set GP0/GP1 as digital I/O
        
        movlw   b'11111110'     ; GP0 as output, others input
        tris    GPIO
        clrf    GPIO            ; Set GP0 low
        
        ; Configure Timer0 (Internal Clock, 1:256 prescaler)
        ; TMR0 increments every 256us. Bit 7 toggles every 32.7ms.
        movlw   b'11010111'
        option

        clrf    SongIndex

; ------------------------------------------------------------------------------
; Main Sequence
; ------------------------------------------------------------------------------
MainLoop:
        ; Fetch note from song table
        movf    SongIndex, W
        call    SongData
        movwf   NoteVal
        
        ; Check for end of song (255)
        incf    NoteVal, W
        btfsc   STATUS, Z
        goto    RestartSong

        ; Set Duration (Long vs Short) based on bit 7
        movlw   d'3'            ; Short note (3 rollovers = ~195ms)
        btfsc   NoteVal, 7
        movlw   d'6'            ; Long note (6 rollovers = ~390ms)
        movwf   DurationCount
        
        ; Mask out duration bit
        movlw   0x7F
        andwf   NoteVal, F
        
        ; Check for rest
        movf    NoteVal, F
        btfsc   STATUS, Z
        goto    PlayRest

        ; Initialize Timer0 state tracking
        movf    TMR0, W
        andlw   b'10000000'
        movwf   TMR0_Last

PlayNoteInner:
        ; Toggle GP0 output
        movlw   b'00000001'
        xorwf   GPIO, F
        
        ; Fetch frequency delay count
        movf    NoteVal, W
        call    FreqData
        movwf   DelayCount

DelayLoop:
        nop
        decfsz  DelayCount, F
        goto    DelayLoop
        
        ; Check Timer0 for duration expiration (1->0 transition of bit 7)
        movf    TMR0, W
        movwf   Temp
        xorwf   TMR0_Last, W
        movwf   Temp2
        btfss   Temp2, 7
        goto    PlayNoteInner   ; Bit 7 did not change
        
        movf    Temp, W
        andlw   b'10000000'
        movwf   TMR0_Last
        btfsc   TMR0_Last, 7
        goto    PlayNoteInner   ; Bit 7 changed from 0 to 1
        
        ; Bit 7 changed from 1 to 0 (Rollover)
        decfsz  DurationCount, F
        goto    PlayNoteInner
        goto    NextNote

PlayRest:
        movf    TMR0, W
        andlw   b'10000000'
        movwf   TMR0_Last
        clrf    GPIO            ; Ensure speaker is off
RestInner:
        movf    TMR0, W
        movwf   Temp
        xorwf   TMR0_Last, W
        movwf   Temp2
        btfss   Temp2, 7
        goto    RestInner
        
        movf    Temp, W
        andlw   b'10000000'
        movwf   TMR0_Last
        btfsc   TMR0_Last, 7
        goto    RestInner
        
        decfsz  DurationCount, F
        goto    RestInner
        goto    NextNote

NextNote:
        clrf    GPIO
        incf    SongIndex, F
        
        ; Small pause between notes (1 rollover = ~65ms)
        movlw   d'1'
        movwf   DurationCount
        movf    TMR0, W
        andlw   b'10000000'
        movwf   TMR0_Last
PauseInner:
        movf    TMR0, W
        movwf   Temp
        xorwf   TMR0_Last, W
        movwf   Temp2
        btfss   Temp2, 7
        goto    PauseInner
        
        movf    Temp, W
        andlw   b'10000000'
        movwf   TMR0_Last
        btfsc   TMR0_Last, 7
        goto    PauseInner
        
        decfsz  DurationCount, F
        goto    PauseInner
        goto    MainLoop

RestartSong:
        ; Long pause before restarting (15 rollovers = ~1 sec)
        movlw   d'15'
        movwf   DurationCount
        movf    TMR0, W
        andlw   b'10000000'
        movwf   TMR0_Last
        clrf    GPIO
RestartInner:
        movf    TMR0, W
        movwf   Temp
        xorwf   TMR0_Last, W
        movwf   Temp2
        btfss   Temp2, 7
        goto    RestartInner
        
        movf    Temp, W
        andlw   b'10000000'
        movwf   TMR0_Last
        btfsc   TMR0_Last, 7
        goto    RestartInner
        
        decfsz  DurationCount, F
        goto    RestartInner
        
        clrf    SongIndex
        goto    MainLoop

; ------------------------------------------------------------------------------
; Frequency Table (Octave 5)
; ------------------------------------------------------------------------------
FreqData:
        addwf   PCL, F
        retlw   0
        retlw   d'235'  ; 1: C5 (523 Hz)
        retlw   d'209'  ; 2: D5 (587 Hz)
        retlw   d'186'  ; 3: E5 (659 Hz)
        retlw   d'175'  ; 4: F5 (698 Hz)
        retlw   d'156'  ; 5: G5 (784 Hz)
        retlw   d'139'  ; 6: A5 (880 Hz)

; ------------------------------------------------------------------------------
; Song Data (Kaeru no Uta)
; ------------------------------------------------------------------------------
SongData:
        movwf   Temp
        movf    Temp, W
        addwf   PCL, F
        
        ; ド レ ミ ファ ミ レ ド (休)
        retlw   N_C + LNG
        retlw   N_D + LNG
        retlw   N_E + LNG
        retlw   N_F + LNG
        retlw   N_E + LNG
        retlw   N_D + LNG
        retlw   N_C + LNG
        retlw   N_R + LNG
        
        ; ミ ファ ソ ラ ソ ファ ミ (休)
        retlw   N_E + LNG
        retlw   N_F + LNG
        retlw   N_G + LNG
        retlw   N_A + LNG
        retlw   N_G + LNG
        retlw   N_F + LNG
        retlw   N_E + LNG
        retlw   N_R + LNG
        
        ; ド (休) ド (休) ド (休) ド (休)
        retlw   N_C + LNG
        retlw   N_R + LNG
        retlw   N_C + LNG
        retlw   N_R + LNG
        retlw   N_C + LNG
        retlw   N_R + LNG
        retlw   N_C + LNG
        retlw   N_R + LNG
        
        ; ド ド レ レ ミ ミ ファ ファ
        retlw   N_C
        retlw   N_C
        retlw   N_D
        retlw   N_D
        retlw   N_E
        retlw   N_E
        retlw   N_F
        retlw   N_F
        
        ; ミ レ ド (休)
        retlw   N_E + LNG
        retlw   N_D + LNG
        retlw   N_C + LNG
        retlw   N_R + LNG
        
        retlw   255     ; End of Song

        END