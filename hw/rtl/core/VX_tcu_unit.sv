// Copyright © 2019-2023
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

`include "VX_define.vh"

module VX_tcu_unit import VX_fpu_pkg::*; #(
    parameter CORE_ID = 0
) (
    input wire clk,
    input wire reset,

    VX_dispatch_if.slave    dispatch_if [`ISSUE_WIDTH],
    VX_tcu_to_csr_if.master tcu_to_csr_if[`NUM_FPU_BLOCKS],
    VX_tcu_to_lsu_if.master tcu_to_lsu_if,  //Check size

    VX_commit_if.master     commit_if [`ISSUE_WIDTH]
);
    `UNUSED_PARAM (CORE_ID)
    localparam BLOCK_SIZE = `ISSUE_WIDTH;
    localparam NUM_LANES  = `NUM_THREADS;
    
    //localparam PID_BITS   = `CLOG2(`NUM_THREADS / NUM_LANES);
    //localparam PID_WIDTH  = `UP(PID_BITS);
    //localparam TAG_WIDTH  = `LOG2UP(`FPUQ_SIZE);
    localparam PARTIAL_BW = (BLOCK_SIZE != `ISSUE_WIDTH) || (NUM_LANES != `NUM_THREADS);

    reg [1:0] count;

    VX_execute_if #(
        .NUM_LANES (NUM_LANES)
    ) execute_if[BLOCK_SIZE]();

    `RESET_RELAY (dispatch_reset, reset);

    VX_dispatch_unit #(
        .BLOCK_SIZE (BLOCK_SIZE),
        .NUM_LANES  (NUM_LANES),
        .OUT_REG    (PARTIAL_BW ? 1 : 0)
    ) dispatch_unit (
        .clk        (clk),
        .reset      (dispatch_reset),
        .dispatch_if(dispatch_if),
        .execute_if (execute_if)
    );

    VX_commit_if #(
        .NUM_LANES (NUM_LANES)
    ) commit_block_if[BLOCK_SIZE]();

    for (genvar block_idx = 0; block_idx < BLOCK_SIZE; ++block_idx) begin
        `UNUSED_VAR (execute_if[block_idx].data.tid)
        `UNUSED_VAR (execute_if[block_idx].data.wb)
        `UNUSED_VAR (execute_if[block_idx].data.use_PC)
        `UNUSED_VAR (execute_if[block_idx].data.use_imm)

        // Store request info
        wire fpu_req_valid, fpu_req_ready;
        wire fpu_rsp_valid, fpu_rsp_ready;    
        wire [NUM_LANES-1:0][`XLEN-1:0] fpu_rsp_result;
        //fflags_t fpu_rsp_fflags;
        //wire fpu_rsp_has_fflags;

        wire [TAG_WIDTH-1:0] fpu_req_tag, fpu_rsp_tag;    
        wire mdata_full;

        wire [`INST_FMT_BITS-1:0] fpu_fmt = execute_if[block_idx].data.imm[`INST_FMT_BITS-1:0];
        wire [`INST_FRM_BITS-1:0] fpu_frm = execute_if[block_idx].data.op_mod[`INST_FRM_BITS-1:0];

        wire execute_fire = execute_if[block_idx].valid && execute_if[block_idx].ready;
        wire fpu_rsp_fire = fpu_rsp_valid && fpu_rsp_ready;

        VX_index_buffer #(
            .DATAW  (`UUID_WIDTH + `NW_WIDTH + NUM_LANES + `XLEN + `NR_BITS + PID_WIDTH + 1 + 1),
            .SIZE   (`FPUQ_SIZE)
        ) tag_store (
            .clk          (clk),
            .reset        (reset),
            .acquire_en   (execute_fire), 
            .write_addr   (fpu_req_tag), 
            .write_data   ({execute_if[block_idx].data.uuid, execute_if[block_idx].data.wid, execute_if[block_idx].data.tmask, execute_if[block_idx].data.PC, execute_if[block_idx].data.rd, execute_if[block_idx].data.pid, execute_if[block_idx].data.sop, execute_if[block_idx].data.eop}),
            .read_data    ({fpu_rsp_uuid, fpu_rsp_wid, fpu_rsp_tmask, fpu_rsp_PC, fpu_rsp_rd, fpu_rsp_pid, fpu_rsp_sop, fpu_rsp_eop}),
            .read_addr    (fpu_rsp_tag),
            .release_en   (fpu_rsp_fire), 
            .full         (mdata_full),
            `UNUSED_PIN (empty)
        );

        // resolve dynamic FRM from CSR   
        wire [`INST_FRM_BITS-1:0] fpu_req_frm; 
        `ASSIGN_BLOCKED_WID (fpu_to_csr_if[block_idx].read_wid, execute_if[block_idx].data.wid, block_idx, `NUM_FPU_BLOCKS)
        assign fpu_req_frm = (execute_if[block_idx].data.op_type != `INST_FPU_MISC 
                           && fpu_frm == `INST_FRM_DYN) ? fpu_to_csr_if[block_idx].read_frm : fpu_frm;

        // submit FPU request

        assign fpu_req_valid = execute_if[block_idx].valid && ~mdata_full;
        assign execute_if[block_idx].ready = fpu_req_ready && ~mdata_full;

        `RESET_RELAY (fpu_reset, reset);   

    `RESET_RELAY (tcu_reset, reset);
    reg [2:0] state;
    reg [2:0] next_state;
    reg [1:0] index_ctr;

    reg tensor_load_done;
    reg tensor_store_start;
    reg tensor_store_done;
    reg mult_done;
    reg [31:0] data_in_a;
    reg [31:0] data_in_b;
    reg [31:0] data_out_c;
    
    always @(posedge clk)
    begin
        if (~tcu_reset) 
        begin
            state        = 3'b0;
            next_state   = 3'b0;
        end
        else
        begin
            next_state <= state;  
        end
    end

    always @(posedge clk) 
    begin
        if (~reset) 
        begin
            //reset LSU/gather signals
            tensor_load_start = 1'b0;
            data_in_a = 32'b0;
            data_in_b = 32'b0;
            state = 3'b0;
            next_state = 3'b0;
        end
        else
        begin
            case (state)
                3'b0 : //data from dcache to GPR
                    begin
                        if(execute_if.valid) 
                        begin
                            //valid to lsu
                            tcu_to_lsu_if.ready = 1'b1;
                            tcu_to_lsu_if.load  = 1'b1;
                            //tcu_to_lsu_if.addr = execute_if.rs1_data; //Check this 

                            //Resp from lsu
                            if(tcu_to_lsu_if.valid) 
                            begin
                                tcu_to_lsu_if.ready = 1'b0;
                                next_state = 3'b1;
                            end
                        end
                    end
                3'b1 : //data from GPR to CSR
                    begin
                        //...
                        //if GPR to CSR is done
                        next_state = 3'b2;
                        tensor_load_start = 1'b1; 
                    end
                3'b2 : //data from CSR to TCU
                    begin 
                        //move data from csr to tcu
                        tcu_to_csr_if[block_idx].read_enable = 1'b1;
                        
                        //Get correct
                        data_in_a = tcu_to_csr_if[block_idx].read_data_a;
                        data_in_b = tcu_to_csr_if[block_idx].read_data_b;

                        //if CSR to TCU is done
                        if (tensor_load_done) 
                        begin
                            next_state = 3'b3;
                        end
                    end
                3'b3 : //Wait for mult
                    begin 
                        if (mult_done) 
                            next_state = 3'b4;
                    end
                3'b4 : //data from TCU to CSR
                    begin
                        if (tensor_store_done != 1)
                        begin
                            //...
                            tcu_to_csr_if[block_idx].write_enable = 1'b1;
                            tcu_to_csr_if[block_idx].write_data   = data_out_c;
                        end
                        if (index_ctr == 2'b3)
                            next_state = 3'b5;
                    end
                3'b5 : //data from CSR to Reg
                    begin
                        //...
                        //if CSR to Reg is done
                        next_state = 3'b6;
                    end
                3'b6 : //data from reg to DCache
                    begin
                        //...
                        tcu_to_lsu_if.ready = 1'b1;
                        tcu_to_lsu_if.load  = 1'b0;                 //Store
                        //tcu_to_lsu_if.addr = execute_if.rs1_data; //Check this 

                        //if LSU is done
                        if(tcu_to_lsu_if.valid)
                        begin
                            tcu_to_lsu_if.ready = 1'b0;
                            next_state = 3'b0;
                        end

                        //Gather valid is set high
                    end
            endcase
        end
    end

    VX_tensor_unit #( 
        .NUM_LANES      (NUM_LANES),
    ) tcu_unit 
    (
        .clk                        (clk),
        .reset                      (tcu_reset),
        .tensor_load_start          (tensor_load_start),
        .tensor_load_done           (tensor_load_done),
        .tensor_store_start         (tensor_store_start),
        .tensor_store_done          (tensor_store_done),
        .tensor_execute_done        (mult_done),
        .data_in_a                  (data_in_a),
        .data_in_b                  (data_in_b),
        .data_out_c                 (data_out_c)
    );
            

        // handle FPU response

        fflags_t fpu_rsp_fflags_q;

        if (PID_BITS != 0) begin
            fflags_t fpu_rsp_fflags_r;
            always @(posedge clk) begin
                if (reset) begin
                    fpu_rsp_fflags_r <= '0;
                end else if (fpu_rsp_fire) begin
                    fpu_rsp_fflags_r <= fpu_rsp_eop ? '0 : (fpu_rsp_fflags_r | fpu_rsp_fflags);
                end
            end
            assign fpu_rsp_fflags_q = fpu_rsp_fflags_r | fpu_rsp_fflags;
        end else begin
            assign fpu_rsp_fflags_q = fpu_rsp_fflags;
        end
        
        assign fpu_to_csr_if[block_idx].write_enable = fpu_rsp_fire && fpu_rsp_eop && fpu_rsp_has_fflags;
        `ASSIGN_BLOCKED_WID (fpu_to_csr_if[block_idx].write_wid, fpu_rsp_wid, block_idx, `NUM_FPU_BLOCKS)
        assign fpu_to_csr_if[block_idx].write_fflags = fpu_rsp_fflags_q;

        // send response

        VX_elastic_buffer #(
            .DATAW (`UUID_WIDTH + `NW_WIDTH + NUM_LANES + `XLEN + `NR_BITS + (NUM_LANES * `XLEN) + PID_WIDTH + 1 + 1),
            .SIZE  (0)
        ) rsp_buf (
            .clk       (clk),
            .reset     (reset),
            .valid_in  (fpu_rsp_valid),
            .ready_in  (fpu_rsp_ready),
            .data_in   ({fpu_rsp_uuid, fpu_rsp_wid, fpu_rsp_tmask, fpu_rsp_PC, fpu_rsp_rd, fpu_rsp_result, fpu_rsp_pid, fpu_rsp_sop, fpu_rsp_eop}),
            .data_out  ({commit_block_if[block_idx].data.uuid, commit_block_if[block_idx].data.wid, commit_block_if[block_idx].data.tmask, commit_block_if[block_idx].data.PC, commit_block_if[block_idx].data.rd, commit_block_if[block_idx].data.data, commit_block_if[block_idx].data.pid, commit_block_if[block_idx].data.sop, commit_block_if[block_idx].data.eop}),
            .valid_out (commit_block_if[block_idx].valid),
            .ready_out (commit_block_if[block_idx].ready)
        );
        assign commit_block_if[block_idx].data.wb = 1'b1;
    end

    `RESET_RELAY (commit_reset, reset);

    VX_gather_unit #(
        .BLOCK_SIZE (BLOCK_SIZE),
        .NUM_LANES  (NUM_LANES),
        .OUT_REG    (PARTIAL_BW ? 3 : 0)
    ) gather_unit (
        .clk           (clk),
        .reset         (commit_reset),
        .commit_in_if  (commit_block_if),
        .commit_out_if (commit_if)
    );

endmodule
