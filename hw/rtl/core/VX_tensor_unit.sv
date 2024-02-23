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

`include "VX_fpu_define.vh"


module VX_tensor_unit #( 
    parameter NUM_LANES = 1,
) (
    input           clk,
    input           reset,

    //data movement control <-> CSRs
    input           tensor_load_start,
    output wire     tensor_load_done,
    output wire     tensor_store_start,
    output wire     tensor_store_done,

    //execution 
    output wire     tensor_execute_done,

    //data
    input   [31:0]  data_in_a,
    input   [31:0]  data_in_b,
    output          data_out_c
);

    //place holder for scratchpad memory
    reg [3:0][31:0] tcore_a;
    reg [3:0][31:0] tcore_b;
    reg [3:0][31:0] tcore_c;

    //iterators
    reg [1:0]       index_ctr;
    genvar i;

    //state registers
    reg     [1:0] tc_state;
    reg     [1:0] tc_next_state;
    
    //control registers
    reg tensor_load_done_reg;
    reg tensor_execute_done_reg;
    reg tensor_store_start_reg;
    reg tensor_store_done_reg;

    //state transition
    always @(@posedge clk) 
    begin
        if (~reset) 
        begin
            generate
                for (i = 0; i < 4; i++)
                begin
                    tcore_a[i] = 32'b0;
                    tcore_b[i] = 32'b0;
                    tcore_c[i] = 32'b0;
                end
            endgenerate                
            tc_state        = 2'b0;
            tc_next_state   = 2'b0;
        end
        else
        begin
            tc_state <= tc_next_state;  
        end
    end

    //logic for outputs
    always @ (posedge clock)
    begin
        if (~reset) 
        begin
            tensor_load_done_reg        = 1'b0;
            tensor_execute_done_reg     = 1'b0;
            tensor_store_start_reg      = 1'b0;
            tensor_store_done_reg       = 1'b0;
        else
        begin
            case (tc_state)
                2'b0 : //moce to state for loading data from CSR
                    begin
                        if (tensor_load_start)
                        begin
                            index_ctr = 0;
                            tc_next_state = 2'b1;
                        end
                    end
                2'b1 : //complete loading from CSR
                    begin
                        tcore_a[index_ctr] = data_in_a;
                        tcore_b[index_ctr] = data_in_b;
                        index_ctr = index_ctr + 1'b1;
                        if (index_ctr == 2'b3)
                        begin
                            index_ctr = 2'b0;
                            tc_next_state = 2'b2;
                            tensor_load_done = 1'b1;
                        end
                    end
                2'b2 : //multiply the matrices (dummy for now)
                    begin 
                        if (index_ctr == 2'b0)
                            tensor_load_done = 1'b0;
                        if (index_ctr == 2'b3)
                        begin
                            index_ctr = 2'b0;
                            tc_next_state = 2'b3;
                            tensor_execute_done = 1'b1;
                        end
                        tcore_c[index_ctr] = tcore_a[index_ctr];
                    end
                2'b3 : //transfer contents from scratchpad to CSRs
                    begin 
                        if (index_ctr == 2'b0)
                            tensor_execute_done = 1'b0;
                            tensor_store_start = 1'b1;
                        else
                            tensor_store_start = 1'b0;
                        if (index_ctr == 2'b3)
                        begin
                            tensor_store_done = 1'b1;
                            tc_next_state = 2'b0;
                            index_ctr = 2'b0;
                        end
                    end
            endcase
        end
    end

    assign tensor_load              = tensor_load_done_reg;
    assign tensor_execute_done      = tensor_execute_done_reg;
    assign tensor_store_start       = tensor_store_start_reg;
    assign tensor_store_done        = tensor_store_done_reg;

endmodule