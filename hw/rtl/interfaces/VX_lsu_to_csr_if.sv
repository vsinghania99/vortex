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

interface VX_lsu_to_csr_if ();

    wire                                read_enable;
    wire [`NUM_THREADS-1:0][31:0]       read_data;  //Check if using NUM_THREADS is correct
    wire [`VX_CSR_ADDR_BITS-1:0]        read_addr;

    wire                                write_enable;
    wire [`NUM_THREADS-1:0][31:0]       write_data; //Check if using NUM_THREADS is correct
    wire [`VX_CSR_ADDR_BITS-1:0]        write_addr;


    //LSU
    modport master (
        output                          write_enable,
        output [31:0]                   write_data,   
        output [`VX_CSR_ADDR_BITS-1:0]  write_addr,

        output                          read_enable,
        input [31:0]                    read_data,
        output [`VX_CSR_ADDR_BITS-1:0]  read_addr
    );

    //CSR
    modport slave (
        input                           write_enable,
        input [31:0]                    write_data,   
        input [`VX_CSR_ADDR_BITS-1:0]   write_addr,

        input                           read_enable,
        output [31:0]                   read_data,
        input [`VX_CSR_ADDR_BITS-1:0]   read_addr
    );

endinterface
