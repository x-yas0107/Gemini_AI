import zipfile
import os

# Content for README.md in Japanese
readme_content = """# TD4 CPU - Altera MAX II (CPLD) 移植プロジェクト・ガイド

本ドキュメントは、書籍『CPUの創りかた』に登場する4ビットCPU「TD4」を、Altera (Intel) MAX II CPLD（EPM240T100C5等）に移植するための設計指針と実装コードをまとめたものです。

## 1. システム構成案

本プロジェクトでは、TD4の「ロジック（頭脳）」をCPLD内部に収めつつ、「プログラム格納部（ROM）」や「クロック制御」を外部インターフェースで拡張するハイブリッド構成をとります。

### 構成の特徴
- **内部ロジック**: ALU、レジスタ(A/B)、プログラムカウンタ(PC)、命令デコーダをVerilog HDLで実装。
- **内部オシレータ**: MAX II内蔵のUFMオシレータを使用し、外部クリスタルなしで動作。
- **外部周波数選択**: 外部スイッチにより、動作スピード（クロック分周比）をリアルタイムに変更可能。
- **外部ROMインターフェース**: ダイオードマトリクス、またはEEPROMを外部に接続可能。

## 2. 内部ロジック (Verilog HDL)

以下のコードは、MAX IIの内部オシレータと分周器を含むTD4のトップモジュールです。

```verilog
// TD4 for Altera MAX II
module td4_top (
    input  wire       n_reset,    // リセット（負論理）
    input  wire [1:0] freq_sel,   // 分周選択スイッチ
    input  wire [7:0] rom_data,   // 外部ROMからの入力 (OP[7:4], Im[3:0])
    input  wire [3:0] sw_in,      // 入力ポート用スイッチ
    output wire [3:0] pc_out,     // 外部ROM用アドレス
    output reg  [3:0] out_port,   // 出力ポートLED
    output wire       cpu_clk_led // クロックモニタLED
);

    // 内部オシレータ (約5MHz)
    wire osc_out;
    maxii_ufm_osc osc_inst (.oscena(1'b1), .osc(osc_out));

    // 分周器
    reg [25:0] divider;
    always @(posedge osc_out or negedge n_reset) begin
        if (!n_reset) divider <= 26'd0;
        else          divider <= divider + 1'b1;
    end

    // クロック選択 (外部入力ピン freq_sel で切り替え)
    wire cpu_clk = (freq_sel == 2'b00) ? divider[24] : // 0.15Hz
                   (freq_sel == 2'b01) ? divider[21] : // 1.2Hz
                   (freq_sel == 2'b10) ? divider[18] : // 10Hz
                                         divider[15];  // 80Hz
    assign cpu_clk_led = cpu_clk;

    // CPUコア
    reg [3:0] reg_a, reg_b, pc;
    reg       c_flag;
    assign pc_out = pc;

    always @(posedge cpu_clk or negedge n_reset) begin
        if (!n_reset) begin
            reg_a <= 0; reg_b <= 0; pc <= 0; out_port <= 0; c_flag <= 0;
        end else begin
            casez (rom_data[7:4]) // OP Code
                4'b0000: {c_flag, reg_a} <= reg_a + rom_data[3:0]; // ADD A, Im
                4'b0101: {c_flag, reg_b} <= reg_b + rom_data[3:0]; // ADD B, Im
                4'b0011: begin reg_a <= rom_data[3:0]; c_flag <= 0; end // MOV A, Im
                4'b0111: begin reg_b <= rom_data[3:0]; c_flag <= 0; end // MOV B, Im
                4'b0001: begin reg_a <= reg_b; c_flag <= 0; end         // MOV A, B
                4'b0100: begin reg_b <= reg_a; c_flag <= 0; end         // MOV B, A
                4'b1111: begin pc <= rom_data[3:0]; c_flag <= 0; end    // JMP Im
                4'b1110: begin if(!c_flag) pc <= rom_data[3:0]; else pc <= pc+1; c_flag <= 0; end // JNC Im
                4'b0010: begin reg_a <= sw_in; c_flag <= 0; end         // IN A
                4'b0110: begin reg_b <= sw_in; c_flag <= 0; end         // IN B
                4'b1011: begin out_port <= rom_data[3:0]; c_flag <= 0; end // OUT Im
                4'b1001: begin out_port <= reg_b; c_flag <= 0; end      // OUT B
                default: pc <= pc + 1'b1;
            endcase
            if (rom_data[7:6] != 2'b11) pc <= pc + 1'b1;
        end
    end
endmodule