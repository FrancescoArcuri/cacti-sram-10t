/*------------------------------------------------------------
 *                              CACTI 6.5
 *         Copyright 2008 Hewlett-Packard Development Corporation
 *                         All Rights Reserved
 *
 * Permission to use, copy, and modify this software and its documentation is
 * hereby granted only under the following terms and conditions.  Both the
 * above copyright notice and this permission notice must appear in all copies
 * of the software, derivative works or modified versions, and any portions
 * thereof, and both notices must appear in supporting documentation.
 *
 * Users of this software agree to the terms and conditions set forth herein, and
 * hereby grant back to Hewlett-Packard Company and its affiliated companies ("HP")
 * a non-exclusive, unrestricted, royalty-free right and license under any changes, 
 * enhancements or extensions  made to the core functions of the software, including 
 * but not limited to those affording compatibility with other hardware or software
 * environments, but excluding applications which incorporate this software.
 * Users further agree to use their best efforts to return to HP any such changes,
 * enhancements or extensions that they make and inform HP of noteworthy uses of
 * this software.  Correspondence should be provided to HP at:
 *
 *                       Director of Intellectual Property Licensing
 *                       Office of Strategy and Technology
 *                       Hewlett-Packard Company
 *                       1501 Page Mill Road
 *                       Palo Alto, California  94304
 *
 * This software may be distributed (but not offered for sale or transferred
 * for compensation) to third parties, provided such third parties agree to
 * abide by the terms and conditions of this notice.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND HP DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL HP 
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 *------------------------------------------------------------*/

#include "area.h"
#include "decoder.h"
#include "parameter.h"
#include <iostream>
#include <math.h>
#include <assert.h>

using namespace std;


Decoder::Decoder(
    int    _num_dec_signals,
    bool   flag_way_select,
    double _C_ld_dec_out,
    double _C_ld_dec_out_rd,
    double _R_wire_dec_out,
    bool   fully_assoc_,
    bool   is_dram_,
    bool   is_wl_tr_,
    const  Area & cell_)
:exist(false),
  C_ld_dec_out(_C_ld_dec_out),
  C_ld_dec_out_rd(_C_ld_dec_out_rd),
  R_wire_dec_out(_R_wire_dec_out),
  num_gates(0), num_gates_rd(0), num_gates_min(2),
  delay(0), delay_rd(0),
  power(), fully_assoc(fully_assoc_), is_dram(is_dram_),
  is_wl_tr(is_wl_tr_), cell(cell_)
{
  for (int i = 0; i < MAX_NUMBER_GATES_STAGE; i++)
  {
    w_dec_n[i] = 0;
    w_dec_p[i] = 0;
    w_dec_n_rd[i] = 0;
    w_dec_p_rd[i] = 0;
  }

  int num_addr_bits_dec = _log2(_num_dec_signals);

  if (num_addr_bits_dec < 4)
  {
    exist = true;
    if (flag_way_select)
    { //exist = true;
      num_in_signals = 2;
    }
    else
    {
      num_in_signals = 0;
    }
  }
  else
  {
    exist = true;

    if (flag_way_select)
    {
      num_in_signals = 3;
    }
    else
    {
      num_in_signals = 2;
    }
  }

  // the height of a row-decoder-driver cell is fixed to be 4 * cell.h;
  //area.h = 4 * cell.h;
  area.h = g_tp.h_dec * cell.h;

  compute_widths();
  compute_area();
}




void Decoder::compute_widths()
{
  double F, g;
  double p_to_n_sz_ratio = pmos_to_nmos_sz_ratio(is_dram, is_wl_tr);
  double gnand2 = (2 + p_to_n_sz_ratio)   / (1 + p_to_n_sz_ratio);
  double gnand3 = (3 + p_to_n_sz_ratio)   / (1 + p_to_n_sz_ratio);
  double gnor2  = (1 + 2*p_to_n_sz_ratio) / (1 + p_to_n_sz_ratio); // Alireza: logical effort of NOR2 gate for DEMUX used in 8T SRAM cell
  double ginv = 1; // Francesco: logical effort for an inverter which is tipically 1 for an INV gate
  double w_nand_n, w_nand_p; // Alireza: transistor widths of the NAND gate in the decoder
  double w_nor_n, w_nor_p; // Alireza: transistor widths of the NOR gate after the decoder in 8T SRAM cell
  double w_inv_n, w_inv_p; // Francesco: transistor widths of the INV gate in the section related to the RWL within the demux of the 10T SRAM cell, why the NOR gate was replaced with an INV gate

  if (exist)
  {
    if (num_in_signals == 2 || fully_assoc)
    {
      w_dec_n[0] = 2 * g_tp.min_w_nmos_;
      w_dec_p[0] = p_to_n_sz_ratio * g_tp.min_w_nmos_;
      g = gnand2;
    }
    else
    {
      w_dec_n[0] = 3 * g_tp.min_w_nmos_;
      w_dec_p[0] = p_to_n_sz_ratio * g_tp.min_w_nmos_;
      g = gnand3;
    }
    
    if (g_ip->sram_cell_design.getType()==std_8T && is_wl_tr) { // Alireza: this 'if-else', and the body of 'if' was added by me! 
      // row decoder for 8T SRAM cell has a DEMUX + WWL and RWL drivers
      w_nand_n = w_dec_n[0];
      w_nand_p = w_dec_p[0];
      w_nor_n = g_tp.min_w_nmos_;
      w_nor_p = 2 * p_to_n_sz_ratio * g_tp.min_w_nmos_;
      g = gnor2;
      
      // calculating optimal number of gates for WWL driver
      w_dec_n[0] = w_nor_n; w_dec_p[0] = w_nor_p; // here the first gate before the inverters (driver) is a NOR2 gate (DEMUX)
      F = g * C_ld_dec_out / (gate_C(w_dec_n[0], 0, is_dram, false, is_wl_tr) +
                              gate_C(w_dec_p[0], 0, is_dram, false, is_wl_tr));
      num_gates = logical_effort(
          num_gates_min,
          g,
          F,
          w_dec_n,
          w_dec_p,
          C_ld_dec_out,
          p_to_n_sz_ratio,
          is_dram,
          is_wl_tr,
          g_tp.max_w_nmos_dec);
      w_dec_n[0] = w_nand_n; w_dec_p[0] = w_nand_p; // returning the first gate to NAND gate for predecode calculation
          
      // calculating optimal number of gates for RWL driver
      w_dec_n_rd[0] = w_nor_n; w_dec_p_rd[0] = w_nor_p; // here the first gate before the inverters (driver) is a NOR2 gate (DEMUX)
      F = g * C_ld_dec_out_rd / (gate_C(w_dec_n_rd[0], 0, is_dram, false, is_wl_tr) +
                                 gate_C(w_dec_p_rd[0], 0, is_dram, false, is_wl_tr));
      num_gates_rd = logical_effort(
          num_gates_min,
          g,
          F,
          w_dec_n_rd,
          w_dec_p_rd,
          C_ld_dec_out_rd,
          p_to_n_sz_ratio,
          is_dram,
          is_wl_tr,
          g_tp.max_w_nmos_dec);
      w_dec_n_rd[0] = w_nand_n; w_dec_p_rd[0] = w_nand_p; // returning the first gate to NAND gate for predecode calculation
    } else if (g_ip->sram_cell_design.getType()==std_10T && is_wl_tr){ //Francesco: this part of if-else was added by me
      // row decoder for 10T SRAM cell has a DEMUX + WWL and RWL drivers
      w_nand_n = w_dec_n[0];
      w_nand_p = w_dec_p[0];
      w_nor_n = g_tp.min_w_nmos_;
      w_nor_p = 2 * p_to_n_sz_ratio * g_tp.min_w_nmos_;
      w_inv_n = g_tp.min_w_nmos_; // Francesco: setting for inverter NMOS
      w_inv_p = p_to_n_sz_ratio * g_tp.min_w_nmos_; // Francesco: setting for inverter PMOS
      g = gnor2;
      
      // calculating optimal number of gates for WWL driver
      w_dec_n[0] = w_nor_n; w_dec_p[0] = w_nor_p; // Francesco: here the first gate before the inverters (driver) is a NOR2 gate (DEMUX)
      F = g * C_ld_dec_out / (gate_C(w_dec_n[0], 0, is_dram, false, is_wl_tr) +
                              gate_C(w_dec_p[0], 0, is_dram, false, is_wl_tr));
      num_gates = logical_effort(
          num_gates_min,
          g,
          F,
          w_dec_n,
          w_dec_p,
          C_ld_dec_out,
          p_to_n_sz_ratio,
          is_dram,
          is_wl_tr,
          g_tp.max_w_nmos_dec);
      w_dec_n[0] = w_nand_n; w_dec_p[0] = w_nand_p; // returning the first gate to NAND gate for predecode calculation
          
      g = ginv;  //Francesco
      // calculating optimal number of gates for RWL driver
      w_dec_n_rd[0] = w_inv_n; w_dec_p_rd[0] = w_inv_p; // Francesco: here the first gate before the inverters (driver) is an INV gate (DEMUX)
      F = g * C_ld_dec_out_rd / (gate_C(w_dec_n_rd[0], 0, is_dram, false, is_wl_tr) +
                                 gate_C(w_dec_p_rd[0], 0, is_dram, false, is_wl_tr));
      num_gates_rd = logical_effort(
          num_gates_min,
          g,
          F,
          w_dec_n_rd,
          w_dec_p_rd,
          C_ld_dec_out_rd,
          p_to_n_sz_ratio,
          is_dram,
          is_wl_tr,
          g_tp.max_w_nmos_dec);
      w_dec_n_rd[0] = w_nand_n; w_dec_p_rd[0] = w_nand_p; // returning the first gate to NAND gate for predecode calculation
    }else { // not the row decoder, or not using 8T/10T SRAM cell
      F = g * C_ld_dec_out / (gate_C(w_dec_n[0], 0, is_dram, false, is_wl_tr) +
                              gate_C(w_dec_p[0], 0, is_dram, false, is_wl_tr));
      num_gates = logical_effort(
          num_gates_min,
          g, // g = (num_in_signals == 2 ? gnand2 : gnand3)
          F,
          w_dec_n,
          w_dec_p,
          C_ld_dec_out,
          p_to_n_sz_ratio,
          is_dram,
          is_wl_tr,
          g_tp.max_w_nmos_dec);
    }
  }
}



void Decoder::compute_area()
{
  double cumulative_area = 0;
  double cumulative_curr = 0;  // cumulative leakage current

  if (exist)
  { // First check if this decoder exists
  
    // Alireza: add area and leakage current of the decoder (one NAND2, or one NAND3)
    if (num_in_signals == 2 || fully_assoc)
    {
      cumulative_area = compute_gate_area(NAND, 2, w_dec_p[0], w_dec_n[0], area.h);  
      cumulative_curr = cmos_Ileak(w_dec_n[0], w_dec_p[0], is_dram) * NAND2_LEAK_STACK_FACTOR;
    }
    else if (num_in_signals == 3)
    {
      cumulative_area = compute_gate_area(NAND, 3, w_dec_p[0], w_dec_n[0], area.h);
      cumulative_curr = cmos_Ileak(w_dec_n[0], w_dec_p[0], is_dram) * NAND3_LEAK_STACK_FACTOR;
    }

    // Alireza: add area and leakage current of the WL (or WWL) driver
    for (int i = 1; i < num_gates; i++)
    {
      cumulative_area += compute_gate_area(INV, 1, w_dec_p[i], w_dec_n[i], area.h);
      cumulative_curr += cmos_Ileak(w_dec_n[i], w_dec_p[i], is_dram) * INV_LEAK_STACK_FACTOR;
    }
    
    // Alireza: add area and leakage current of the DEMUX (i.e., one NOR2 gate), and RWL driver
    if (g_ip->sram_cell_design.getType()==std_8T && is_wl_tr) {
      double p_to_n_sz_ratio = pmos_to_nmos_sz_ratio(is_dram, is_wl_tr);
      double w_nor_n = g_tp.min_w_nmos_;
      double w_nor_p = 2 * p_to_n_sz_ratio * g_tp.min_w_nmos_;
      cumulative_area += 2 * compute_gate_area(NOR, 2, w_nor_p, w_nor_n, area.h);  //Francesco: add a factor of 2 as a multiplier because I consider the two NOR gates present in the demux
      cumulative_curr += 2 * cmos_Ileak(w_nor_n, w_nor_p, is_dram) * NOR2_LEAK_STACK_FACTOR;  //Francesco: add a factor of 2 as a multiplier because I consider the two NOR gates present in the demux
      for (int i = 1; i < num_gates_rd; i++) {
        cumulative_area += compute_gate_area(INV, 1, w_dec_p_rd[i], w_dec_n_rd[i], area.h);
        cumulative_curr += cmos_Ileak(w_dec_n_rd[i], w_dec_p_rd[i], is_dram) * INV_LEAK_STACK_FACTOR;
      }
    } else if (g_ip->sram_cell_design.getType()==std_10T && is_wl_tr) {
      double p_to_n_sz_ratio = pmos_to_nmos_sz_ratio(is_dram, is_wl_tr);
      double w_nor_n = g_tp.min_w_nmos_;
      double w_nor_p = 2 * p_to_n_sz_ratio * g_tp.min_w_nmos_;
      cumulative_area += compute_gate_area(NOR, 2, w_nor_p, w_nor_n, area.h);  
      cumulative_curr += cmos_Ileak(w_nor_n, w_nor_p, is_dram) * NOR2_LEAK_STACK_FACTOR;
      //Francesco: Add the inverter's area and leakage current present in the demux before the RWL
      double w_inv_n = g_tp.min_w_nmos_;
      double w_inv_p = p_to_n_sz_ratio * g_tp.min_w_nmos_;
      cumulative_area += compute_gate_area(INV, 1, w_inv_p, w_inv_n, area.h);
      cumulative_curr += cmos_Ileak(w_inv_n, w_inv_p, is_dram) * INV_LEAK_STACK_FACTOR;
      for (int i = 1; i < num_gates_rd; i++) {
        cumulative_area += compute_gate_area(INV, 1, w_dec_p_rd[i], w_dec_n_rd[i], area.h);
        cumulative_curr += cmos_Ileak(w_dec_n_rd[i], w_dec_p_rd[i], is_dram) * INV_LEAK_STACK_FACTOR;
      }
    }
    
    power.readOp.leakage = cumulative_curr * g_tp.peri_global.Vdd;
    area.w = (cumulative_area / area.h);
  }
}


double Decoder::compute_delays(double inrisetime)
{
  if (exist)
  {

    double ret_val = 0;  //Francesco: value of the final inverter that drives the read wordline (RWL)
    int    i;
    double rd, tf, this_delay, c_load, c_intrinsic, Vpp;
    double Vdd = g_tp.peri_global.Vdd;

    if ((is_wl_tr) && (is_dram))
    {
      Vpp = g_tp.vpp;
    }
    else if (is_wl_tr)
    {
      Vpp = g_tp.sram_cell.Vdd;
    }
    else
    {
      Vpp = g_tp.peri_global.Vdd;
    }

    // first check whether a decoder is required at all
    double w_nor_n = g_tp.min_w_nmos_; // Alireza
    double w_nor_p = 2 * pmos_to_nmos_sz_ratio(is_dram, is_wl_tr) * g_tp.min_w_nmos_; // Alireza
    
    
    // Alireza: add delay of the decoder (one NAND2, or one NAND3)
    rd = tr_R_on(w_dec_n[0], NCH, num_in_signals, is_dram, false, is_wl_tr);
    c_intrinsic = drain_C_(w_dec_p[0], PCH, 1, 1, area.h, is_dram, false, is_wl_tr) * num_in_signals + 
                  drain_C_(w_dec_n[0], NCH, num_in_signals, 1, area.h, is_dram, false, is_wl_tr);
    //Alireza & Francesco
    if (g_ip->sram_cell_design.getType()==std_8T && is_wl_tr) {
      // Alireza: for this case, load of the decoder is the DEMUX (a NOR gate)
      c_load = gate_C(w_nor_n + w_nor_p, 0.0, is_dram, false, is_wl_tr); // Alireza
      tf = rd * (c_intrinsic + 2*c_load);
    } else if (g_ip->sram_cell_design.getType()==std_10T && is_wl_tr){ //Francesco
      //the load capacitance is given by a nor and an inverter because, in the 10T SRAM cell, we consider in the demux an inverter
      //before the RWL driver
      c_load = gate_C(w_nor_n + w_nor_p, 0.0, is_dram, false, is_wl_tr) + gate_C(w_dec_n_rd[1] + w_dec_p_rd[1], 0.0, is_dram, false, is_wl_tr);
      tf = rd * (c_intrinsic + c_load);
    } else { // Alireza
      c_load = gate_C(w_dec_n[1] + w_dec_p[1], 0.0, is_dram, false, is_wl_tr);
      tf = rd * (c_intrinsic + c_load);
    } // Alireza
    this_delay = horowitz(inrisetime, tf, 0.5, 0.5, RISE);
    delay += this_delay;
    inrisetime = this_delay / (1.0 - 0.5);  
    if ((g_ip->sram_cell_design.getType()==std_8T || g_ip->sram_cell_design.getType()==std_10T) && is_wl_tr) { // Alireza & Francesco
      power.readOp.dynamic += (c_load + c_intrinsic) * Vdd * Vdd; // Alireza
      power.writeOp.dynamic += power.readOp.dynamic; // Alireza
      delay_rd += delay; // Alireza
      //Francesco: add the decoder power consumption in the readOp option just by convention because there is not reading and writing consumption for the decoder
      power_decoder.readOp.dynamic += (c_load + c_intrinsic) * Vdd * Vdd;
    } else { // Alireza
      power.readOp.dynamic += (c_load + c_intrinsic) * Vdd * Vdd;
      //Francesco: add the decoder power consumption in the readOp option just by convention because there is not reading and writing consumption for the decoder
      power_decoder.readOp.dynamic += (c_load + c_intrinsic) * Vdd * Vdd;
    } // Alireza
    
    // Alireza: add delay of DEMUX (one NOR2 gate)
    if ((g_ip->sram_cell_design.getType()==std_8T || g_ip->sram_cell_design.getType()==std_10T) && is_wl_tr) { // Alireza: this 'if' is added by me!
      rd = tr_R_on(w_nor_n, NCH, 2, is_dram, false, is_wl_tr);
      c_intrinsic = drain_C_(w_nor_p, PCH, 2, 1, area.h, is_dram, false, is_wl_tr) + 
                    drain_C_(w_nor_n, NCH, 1, 1, area.h, is_dram, false, is_wl_tr) * 2;
      c_load = gate_C(w_dec_n[1] + w_dec_p[1], 0.0, is_dram, false, is_wl_tr);
      tf = rd * (c_intrinsic + c_load);
      this_delay = horowitz(inrisetime, tf, 0.5, 0.5, RISE);
      delay += this_delay;
      inrisetime = this_delay / (1.0 - 0.5);
      if (g_ip->sram_cell_design.getType()==std_10T) { //Francesco
        power.writeOp.dynamic += (c_load + c_intrinsic) * Vdd * Vdd;
        //add the nor power consumption to that of the decoder because we consider the demux part of the decoder
        power_decoder.readOp.dynamic += (c_load + c_intrinsic) * Vdd * Vdd; //Francesco
        //add the power consumption of the inverter (considered inside the demux instead of RWL nor) to that of the decoder because, in 10T SRAM cell,
        //the access transistor connected to the RWL is active during the writing operation, so the RWL is active and must be considered
        c_load = gate_C(w_dec_p_rd[1] + w_dec_n_rd[1], 0.0, is_dram, false, is_wl_tr);
        c_intrinsic = drain_C_(w_dec_p_rd[1], PCH, 1, 1, area.h, is_dram, false, is_wl_tr) + 
                      drain_C_(w_dec_n_rd[1], NCH, 1, 1, area.h, is_dram, false, is_wl_tr);
        power.writeOp.dynamic += (c_load + c_intrinsic) * Vdd * Vdd;
        power_decoder.readOp.dynamic += (c_load + c_intrinsic) * Vdd * Vdd;
      } else {
        power.writeOp.dynamic += (c_load + c_intrinsic) * Vdd * Vdd;
        power_decoder.readOp.dynamic += (c_load + c_intrinsic) * Vdd * Vdd; //Francesco
      }
      
    }
    
    // Alireza: add delay of WL (or WWL) driver
    for (i = 1; i < num_gates - 1; ++i) {
      rd = tr_R_on(w_dec_n[i], NCH, 1, is_dram, false, is_wl_tr);
      c_load = gate_C(w_dec_p[i+1] + w_dec_n[i+1], 0.0, is_dram, false, is_wl_tr);
      c_intrinsic = drain_C_(w_dec_p[i], PCH, 1, 1, area.h, is_dram, false, is_wl_tr) + 
                    drain_C_(w_dec_n[i], NCH, 1, 1, area.h, is_dram, false, is_wl_tr);
      tf = rd * (c_intrinsic + c_load);
      this_delay = horowitz(inrisetime, tf, 0.5, 0.5, RISE);
      delay += this_delay;
      inrisetime = this_delay / (1.0 - 0.5);
      if ((g_ip->sram_cell_design.getType()==std_8T || g_ip->sram_cell_design.getType()==std_10T) && is_wl_tr) { // Alireza & Francesco
        power.writeOp.dynamic += (c_load + c_intrinsic) * Vdd * Vdd; // Alireza
        power_wordline.writeOp.dynamic += (c_load + c_intrinsic) * Vdd * Vdd; //Francesco
        if (g_ip->sram_cell_design.getType()==std_10T) { //Francesco
          //add the power consumption of the RWL driver to that of the WWL driver because, in 10T SRAM cell,
          //the access transistor connected to the RWL is active during the writing operation, so the RWL is active and must be considered
          c_load = gate_C(w_dec_p_rd[i+1] + w_dec_n_rd[i+1], 0.0, is_dram, false, is_wl_tr);
          c_intrinsic = drain_C_(w_dec_p_rd[i], PCH, 1, 1, area.h, is_dram, false, is_wl_tr) + 
                        drain_C_(w_dec_n_rd[i], NCH, 1, 1, area.h, is_dram, false, is_wl_tr);
          power.writeOp.dynamic += (c_load + c_intrinsic) * Vdd * Vdd;
          power_wordline.writeOp.dynamic += (c_load + c_intrinsic) * Vdd * Vdd; //Francesco
        }
      } else { // Alireza
        power.readOp.dynamic += (c_load + c_intrinsic) * Vdd * Vdd;
        power_wordline.readOp.dynamic += (c_load + c_intrinsic) * Vdd * Vdd; //Francesco
      } // Alireza
    }

    // add delay of final inverter that drives the wordline (WL, or WWL)
    i = num_gates - 1;
    c_load = C_ld_dec_out;
    rd = tr_R_on(w_dec_n[i], NCH, 1, is_dram, false, is_wl_tr);
    c_intrinsic = drain_C_(w_dec_p[i], PCH, 1, 1, area.h, is_dram, false, is_wl_tr) + 
                  drain_C_(w_dec_n[i], NCH, 1, 1, area.h, is_dram, false, is_wl_tr);
    tf = rd * (c_intrinsic + c_load) + R_wire_dec_out * c_load / 2;
    this_delay = horowitz(inrisetime, tf, 0.5, 0.5, RISE);
    delay  += this_delay;
    ret_val = this_delay / (1.0 - 0.5);
    if ((g_ip->sram_cell_design.getType()==std_8T || g_ip->sram_cell_design.getType()==std_10T) && is_wl_tr) { // Alireza & Francesco
      power.writeOp.dynamic += c_load * Vpp * Vpp + c_intrinsic * Vdd * Vdd; // Alireza
      power_wordline.writeOp.dynamic += c_load * Vpp * Vpp + c_intrinsic * Vdd * Vdd; //Francesco
      if (g_ip->sram_cell_design.getType()==std_10T) { //Francesco
        //add the power consumption of the RWL final inverter to that of the WWL driver because, in 10T SRAM cell,
        //the access transistor connected to the RWL is active during the writing operation, so the RWL is active and must be considered
        i = num_gates_rd - 1;
        c_load = C_ld_dec_out_rd;
        c_intrinsic = drain_C_(w_dec_p_rd[i], PCH, 1, 1, area.h, is_dram, false, is_wl_tr) + 
                      drain_C_(w_dec_n_rd[i], NCH, 1, 1, area.h, is_dram, false, is_wl_tr);
        power.writeOp.dynamic += c_load * Vpp * Vpp + c_intrinsic * Vdd * Vdd;
        power_wordline.writeOp.dynamic += c_load * Vpp * Vpp + c_intrinsic * Vdd * Vdd; //Francesco
      }
    } else { // Alireza
      power.readOp.dynamic += c_load * Vpp * Vpp + c_intrinsic * Vdd * Vdd;
      power_wordline.readOp.dynamic += c_load * Vpp * Vpp + c_intrinsic * Vdd * Vdd; //Francesco
    } // Alireza
    
    
    // Alireza: add delay of RWL driver
    if ((g_ip->sram_cell_design.getType()==std_8T || g_ip->sram_cell_design.getType()==std_10T) && is_wl_tr) { // Alireza (& Francesco): this 'if' is added by me!
      // add delay of DEMUX (one NOR2 gate)
      if (g_ip->sram_cell_design.getType()==std_8T) {
      rd = tr_R_on(w_nor_n, NCH, 2, is_dram, false, is_wl_tr);
      c_load = gate_C(w_dec_n_rd[1] + w_dec_p_rd[1], 0.0, is_dram, false, is_wl_tr);
      c_intrinsic = drain_C_(w_nor_p, PCH, 2, 1, area.h, is_dram, false, is_wl_tr) + 
                    drain_C_(w_nor_n, NCH, 1, 1, area.h, is_dram, false, is_wl_tr) * 2;
      tf = rd * (c_intrinsic + c_load);
      this_delay = horowitz(inrisetime, tf, 0.5, 0.5, RISE);
      delay_rd += this_delay;
      inrisetime = this_delay / (1.0 - 0.5);  
      power.readOp.dynamic += (c_load + c_intrinsic) * Vdd * Vdd;
      //add nor power consumption inside the demux to that of the decoder
      power_decoder.readOp.dynamic += (c_load + c_intrinsic) * Vdd * Vdd; //Francesco
      } else {
        rd = tr_R_on(w_dec_n_rd[1], NCH, 1, is_dram, false, is_wl_tr); //Francesco: ritardo di un inverter che abbiamo messo al posto della nor
        c_load = gate_C(w_dec_n_rd[1] + w_dec_p_rd[1], 0.0, is_dram, false, is_wl_tr);
        c_intrinsic = drain_C_(w_dec_p_rd[1], PCH, 1, 1, area.h, is_dram, false, is_wl_tr) + 
                      drain_C_(w_dec_n_rd[1], NCH, 1, 1, area.h, is_dram, false, is_wl_tr);
        tf = rd * (c_intrinsic + c_load);
        this_delay = horowitz(inrisetime, tf, 0.5, 0.5, RISE);
        delay_rd += this_delay;
        inrisetime = this_delay / (1.0 - 0.5);  
        power.readOp.dynamic += (c_load + c_intrinsic) * Vdd * Vdd;
        //add the power of the inverter inside the demux (for the 10T SRAM cell) to that one of the decoder
        power_decoder.readOp.dynamic += (c_load + c_intrinsic) * Vdd * Vdd; //Francesco
      }


      // Alireza: add delay of RWL driver
      for (i = 1; i < num_gates_rd - 1; ++i)
      {
        rd = tr_R_on(w_dec_n_rd[i], NCH, 1, is_dram, false, is_wl_tr);
        c_load = gate_C(w_dec_p_rd[i+1] + w_dec_n_rd[i+1], 0.0, is_dram, false, is_wl_tr);
        c_intrinsic = drain_C_(w_dec_p_rd[i], PCH, 1, 1, area.h, is_dram, false, is_wl_tr) + 
                      drain_C_(w_dec_n_rd[i], NCH, 1, 1, area.h, is_dram, false, is_wl_tr);
        tf = rd * (c_intrinsic + c_load);
        this_delay = horowitz(inrisetime, tf, 0.5, 0.5, RISE);
        delay_rd += this_delay;
        inrisetime = this_delay / (1.0 - 0.5);
        power.readOp.dynamic += (c_load + c_intrinsic) * Vdd * Vdd;
        power_wordline.readOp.dynamic += (c_load + c_intrinsic) * Vdd * Vdd; //Francesco
      }
      
      // add delay of final inverter that drives the read wordline (RWL)
      i = num_gates_rd - 1;
      c_load = C_ld_dec_out_rd;
      rd = tr_R_on(w_dec_n_rd[i], NCH, 1, is_dram, false, is_wl_tr);
      c_intrinsic = drain_C_(w_dec_p_rd[i], PCH, 1, 1, area.h, is_dram, false, is_wl_tr) + 
                    drain_C_(w_dec_n_rd[i], NCH, 1, 1, area.h, is_dram, false, is_wl_tr);
      tf = rd * (c_intrinsic + c_load) + R_wire_dec_out * c_load / 2;
      this_delay = horowitz(inrisetime, tf, 0.5, 0.5, RISE);
      delay_rd  += this_delay;
      ret_val = this_delay / (1.0 - 0.5); // Alireza: for 8T SRAM cell return read wordline delay
      power.readOp.dynamic += c_load * Vpp * Vpp + c_intrinsic * Vdd * Vdd;
      power_wordline.readOp.dynamic += c_load * Vpp * Vpp + c_intrinsic * Vdd * Vdd; //Francesco
    }
    
    
    return ret_val;
  }
  else
  {
    return 0.0;
  }
}



PredecBlk::PredecBlk(
    int    num_dec_signals,
    Decoder * dec_,
    double C_wire_predec_blk_out,
    double R_wire_predec_blk_out_,
    int    num_dec_per_predec,
    bool   is_dram,
    bool   is_blk1)
 :dec(dec_),
  exist(false),
  number_input_addr_bits(0),
  C_ld_predec_blk_out(0),
  R_wire_predec_blk_out(0),
  branch_effort_nand2_gate_output(1),
  branch_effort_nand3_gate_output(1),
  flag_two_unique_paths(false),
  flag_L2_gate(0),
  number_inputs_L1_gate(0),
  number_gates_L1_nand2_path(0),
  number_gates_L1_nand3_path(0),
  number_gates_L2(0),
  min_number_gates_L1(2),
  min_number_gates_L2(2),
  delay_nand2_path(0),
  delay_nand3_path(0),
  power_nand2_path(),
  power_nand3_path(),
  power_L2(),
  is_dram_(is_dram)
{
  int    branch_effort_predec_out;
  double C_ld_dec_gate;
  int    num_addr_bits_dec = _log2(num_dec_signals);
  int    blk1_num_input_addr_bits = (num_addr_bits_dec + 1) / 2;
  int    blk2_num_input_addr_bits = num_addr_bits_dec - blk1_num_input_addr_bits;

  w_L1_nand2_n[0] = 0;
  w_L1_nand2_p[0] = 0;
  w_L1_nand3_n[0] = 0;
  w_L1_nand3_p[0] = 0;
  
  if (is_blk1 == true)
  {
    if (num_addr_bits_dec <= 0)
    {
      return;
    }
    else if (num_addr_bits_dec < 4)
    {
      // Just one predecoder block is required with NAND2 gates. No decoder required.
      // The first level of predecoding directly drives the decoder output load
      exist = true;
      number_input_addr_bits = num_addr_bits_dec;
      R_wire_predec_blk_out = dec->R_wire_dec_out;
      C_ld_predec_blk_out = dec->C_ld_dec_out;  
    }
    else
    {
      exist = true;
      number_input_addr_bits   = blk1_num_input_addr_bits;
      branch_effort_predec_out = (1 << blk2_num_input_addr_bits);
      C_ld_dec_gate = num_dec_per_predec * gate_C(dec->w_dec_n[0] + dec->w_dec_p[0], 0, is_dram_, false, false);
      R_wire_predec_blk_out = R_wire_predec_blk_out_;
      C_ld_predec_blk_out = branch_effort_predec_out * C_ld_dec_gate + C_wire_predec_blk_out;
    }
  }
  else
  {
    if (num_addr_bits_dec >= 4)
    {
      exist = true;
      number_input_addr_bits   = blk2_num_input_addr_bits;
      branch_effort_predec_out = (1 << blk1_num_input_addr_bits);
      C_ld_dec_gate = num_dec_per_predec * gate_C(dec->w_dec_n[0] + dec->w_dec_p[0], 0, is_dram_, false, false);
      R_wire_predec_blk_out = R_wire_predec_blk_out_;
      C_ld_predec_blk_out = branch_effort_predec_out * C_ld_dec_gate + C_wire_predec_blk_out;
    }
  }

  compute_widths();
  compute_area();
}



void PredecBlk::compute_widths()
{
  double F, c_load_nand3_path, c_load_nand2_path;
  double p_to_n_sz_ratio = pmos_to_nmos_sz_ratio(is_dram_);
  double gnand2 = (2 + p_to_n_sz_ratio) / (1 + p_to_n_sz_ratio);
  double gnand3 = (3 + p_to_n_sz_ratio) / (1 + p_to_n_sz_ratio);

  if (exist == false) return;


  switch (number_input_addr_bits)
  {
    case 1:
      flag_two_unique_paths           = false;
      number_inputs_L1_gate           = 2;
      flag_L2_gate                    = 0;
      break;
    case 2:
      flag_two_unique_paths           = false;
      number_inputs_L1_gate           = 2;
      flag_L2_gate                    = 0;
      break;
    case 3:
      flag_two_unique_paths           = false;
      number_inputs_L1_gate           = 3;
      flag_L2_gate                    = 0;    
      break;
    case 4:
      flag_two_unique_paths           = false;
      number_inputs_L1_gate           = 2;
      flag_L2_gate                    = 2;  
      branch_effort_nand2_gate_output = 4;
      break;
    case 5:
      flag_two_unique_paths           = true;
      flag_L2_gate                    = 2;  
      branch_effort_nand2_gate_output = 8;
      branch_effort_nand3_gate_output = 4;
      break;
    case 6:
      flag_two_unique_paths           = false;
      number_inputs_L1_gate           = 3;
      flag_L2_gate                    = 2;  
      branch_effort_nand3_gate_output = 8;
      break;
    case 7:
      flag_two_unique_paths           = true;
      flag_L2_gate                    = 3;  
      branch_effort_nand2_gate_output = 32;
      branch_effort_nand3_gate_output = 16;
      break;
    case 8:
      flag_two_unique_paths           = true;
      flag_L2_gate                    = 3;  
      branch_effort_nand2_gate_output = 64;
      branch_effort_nand3_gate_output = 32;
      break;
    case 9:
      flag_two_unique_paths           = false;
      number_inputs_L1_gate           = 3;
      flag_L2_gate                    = 3;
      branch_effort_nand3_gate_output = 64;
      break;
    default:
      assert(0);
      break;
  }

  // find the number of gates and sizing in second level of predecoder (if there is a second level)
  if (flag_L2_gate)
  {
    if (flag_L2_gate == 2)
    { // 2nd level is a NAND2 gate
      w_L2_n[0] = 2 * g_tp.min_w_nmos_;
      F = gnand2;
    }
    else
    { // 2nd level is a NAND3 gate
      w_L2_n[0] = 3 * g_tp.min_w_nmos_;
      F = gnand3;
    }
    w_L2_p[0] = p_to_n_sz_ratio * g_tp.min_w_nmos_;
    F *= C_ld_predec_blk_out / (gate_C(w_L2_n[0], 0, is_dram_) + gate_C(w_L2_p[0], 0, is_dram_));
    number_gates_L2 = logical_effort(
        min_number_gates_L2,
        flag_L2_gate == 2 ? gnand2 : gnand3,
        F,
        w_L2_n,
        w_L2_p,
        C_ld_predec_blk_out,
        p_to_n_sz_ratio,
        is_dram_, false,
        g_tp.max_w_nmos_);

    // Now find the number of gates and widths in first level of predecoder
    if ((flag_two_unique_paths)||(number_inputs_L1_gate == 2))
    { // Whenever flag_two_unique_paths is true, it means first level of decoder employs
      // both NAND2 and NAND3 gates. Or when number_inputs_L1_gate is 2, it means
      // a NAND2 gate is used in the first level of the predecoder
      c_load_nand2_path = branch_effort_nand2_gate_output * 
        (gate_C(w_L2_n[0], 0, is_dram_) + 
         gate_C(w_L2_p[0], 0, is_dram_));
      w_L1_nand2_n[0] = 2 * g_tp.min_w_nmos_;
      w_L1_nand2_p[0] = p_to_n_sz_ratio * g_tp.min_w_nmos_;
      F = gnand2 * c_load_nand2_path / 
        (gate_C(w_L1_nand2_n[0], 0, is_dram_) +
         gate_C(w_L1_nand2_p[0], 0, is_dram_));
      number_gates_L1_nand2_path = logical_effort(
          min_number_gates_L1,
          gnand2,
          F,
          w_L1_nand2_n,
          w_L1_nand2_p,
          c_load_nand2_path,
          p_to_n_sz_ratio,
          is_dram_, false,
          g_tp.max_w_nmos_);
    }

    //Now find widths of gates along path in which first gate is a NAND3
    if ((flag_two_unique_paths)||(number_inputs_L1_gate == 3))
    { // Whenever flag_two_unique_paths is TRUE, it means first level of decoder employs
      // both NAND2 and NAND3 gates. Or when number_inputs_L1_gate is 3, it means
      // a NAND3 gate is used in the first level of the predecoder
      c_load_nand3_path = branch_effort_nand3_gate_output * 
        (gate_C(w_L2_n[0], 0, is_dram_) + 
         gate_C(w_L2_p[0], 0, is_dram_));
      w_L1_nand3_n[0] = 3 * g_tp.min_w_nmos_;
      w_L1_nand3_p[0] = p_to_n_sz_ratio * g_tp.min_w_nmos_;
      F = gnand3 * c_load_nand3_path / 
        (gate_C(w_L1_nand3_n[0], 0, is_dram_) +
         gate_C(w_L1_nand3_p[0], 0, is_dram_));
      number_gates_L1_nand3_path = logical_effort(
          min_number_gates_L1,
          gnand3,
          F,
          w_L1_nand3_n,
          w_L1_nand3_p,
          c_load_nand3_path, 
          p_to_n_sz_ratio,
          is_dram_, false,
          g_tp.max_w_nmos_);
    }  
  }
  else
  { // find number of gates and widths in first level of predecoder block when there is no second level 
    if (number_inputs_L1_gate == 2)
    {
      w_L1_nand2_n[0] = 2 * g_tp.min_w_nmos_;
      w_L1_nand2_p[0] = p_to_n_sz_ratio * g_tp.min_w_nmos_;
      F = C_ld_predec_blk_out / 
        (gate_C(w_L1_nand2_n[0], 0, is_dram_) +
         gate_C(w_L1_nand2_p[0], 0, is_dram_));
      number_gates_L1_nand2_path = logical_effort(
          min_number_gates_L1,
          gnand2,
          F,
          w_L1_nand2_n,
          w_L1_nand2_p,
          C_ld_predec_blk_out,
          p_to_n_sz_ratio,
          is_dram_, false,
          g_tp.max_w_nmos_);
    }
    else if (number_inputs_L1_gate == 3)
    {
      w_L1_nand3_n[0] = 3 * g_tp.min_w_nmos_;
      w_L1_nand3_p[0] = p_to_n_sz_ratio * g_tp.min_w_nmos_;
      F = C_ld_predec_blk_out / 
        (gate_C(w_L1_nand3_n[0], 0, is_dram_) +
         gate_C(w_L1_nand3_p[0], 0, is_dram_));
      number_gates_L1_nand3_path = logical_effort(
          min_number_gates_L1,
          gnand3,
          F,
          w_L1_nand3_n,
          w_L1_nand3_p,
          C_ld_predec_blk_out,
          p_to_n_sz_ratio,
          is_dram_, false,
          g_tp.max_w_nmos_);
    }
  }
}



void PredecBlk::compute_area()
{
  if (exist)
  {
    int num_L1_nand2 = 0;
    int num_L1_nand3 = 0;
    int num_L2 = 0;
    double tot_area_L1_nand3;
    double leak_L1_nand3;

    // First check whether a predecoder block is needed
    double tot_area_L1_nand2 = compute_gate_area(NAND, 2, w_L1_nand2_p[0], w_L1_nand2_n[0], g_tp.cell_h_def); 
    double leak_L1_nand2 = cmos_Ileak(w_L1_nand2_n[0], w_L1_nand2_p[0], is_dram_) * NAND2_LEAK_STACK_FACTOR;
    if (number_inputs_L1_gate != 3) {
      tot_area_L1_nand3 = 0;
      leak_L1_nand3 = 0;
    }
    else {
      tot_area_L1_nand3 = compute_gate_area(NAND, 3, w_L1_nand3_p[0], w_L1_nand3_n[0], g_tp.cell_h_def);
      leak_L1_nand3 = cmos_Ileak(w_L1_nand3_n[0], w_L1_nand3_p[0], is_dram_) * NAND3_LEAK_STACK_FACTOR;
    }

    switch (number_input_addr_bits)
    {
      case 1: //2 NAND2 gates
        num_L1_nand2 = 2;
        num_L2       = 0;
        break;
      case 2: //4 NAND2 gates
        num_L1_nand2 = 4;
        num_L2       = 0;
        break;
      case 3: //8 NAND3 gates
        num_L1_nand3 = 8;
        num_L2       = 0;
        break;
      case 4: //4 + 4 NAND2 gates
        num_L1_nand2 = 8;
        num_L2       = 16;
        break;
      case 5: //4 NAND2 gates, 8 NAND3 gates
        num_L1_nand2 = 4;
        num_L1_nand3 = 8;
        num_L2       = 32;
        break;
      case 6: //8 + 8 NAND3 gates
        num_L1_nand3 = 16;
        num_L2       = 64;
        break;
      case 7: //4 + 4 NAND2 gates, 8 NAND3 gates
        num_L1_nand2 = 8;
        num_L1_nand3 = 8;
        num_L2       = 128;
        break;
      case 8: //4 NAND2 gates, 8 + 8 NAND3 gates
        num_L1_nand2 = 4;
        num_L1_nand3 = 16;
        num_L2       = 256;
        break;
      case 9: //8 + 8 + 8 NAND3 gates
        num_L1_nand3 = 24;
        num_L2       = 512;
        break;
      default:
        break;
    }

    for (int i = 1; i < number_gates_L1_nand2_path; ++i)
    {
      tot_area_L1_nand2 += compute_gate_area(INV, 1, w_L1_nand2_p[i], w_L1_nand2_n[i], g_tp.cell_h_def);  
      leak_L1_nand2 += cmos_Ileak(w_L1_nand2_n[i], w_L1_nand2_p[i], is_dram_) * INV_LEAK_STACK_FACTOR;
    }
    tot_area_L1_nand2 *= num_L1_nand2;
    leak_L1_nand2     *= num_L1_nand2;

    for (int i = 1; i < number_gates_L1_nand3_path; ++i)
    {
      tot_area_L1_nand3 += compute_gate_area(INV, 1, w_L1_nand3_p[i], w_L1_nand3_n[i], g_tp.cell_h_def); 
      leak_L1_nand3 += cmos_Ileak(w_L1_nand3_n[i], w_L1_nand3_p[i], is_dram_) * INV_LEAK_STACK_FACTOR;
    }
    tot_area_L1_nand3 *= num_L1_nand3;
    leak_L1_nand3     *= num_L1_nand3;

    double cumulative_area_L1 = tot_area_L1_nand2 + tot_area_L1_nand3;
    double cumulative_area_L2 = 0.0;
    double leakage_L2         = 0.0;

    if (flag_L2_gate == 2)
    {
      cumulative_area_L2 = compute_gate_area(NAND, 2, w_L2_p[0], w_L2_n[0], g_tp.cell_h_def); 
      leakage_L2         = cmos_Ileak(w_L2_n[0], w_L2_p[0], is_dram_) * NAND2_LEAK_STACK_FACTOR;
    }
    else if (flag_L2_gate == 3)
    {
      cumulative_area_L2 = compute_gate_area(NAND, 3, w_L2_p[0], w_L2_n[0], g_tp.cell_h_def);
      leakage_L2         = cmos_Ileak(w_L2_n[0], w_L2_p[0], is_dram_) * NAND3_LEAK_STACK_FACTOR;
    }

    for (int i = 1; i < number_gates_L2; ++i)
    {
      cumulative_area_L2 += compute_gate_area(INV, 1, w_L2_p[i], w_L2_n[i], g_tp.cell_h_def); 
      leakage_L2         += cmos_Ileak(w_L2_n[i], w_L2_p[i], is_dram_) * INV_LEAK_STACK_FACTOR;
    }
    cumulative_area_L2 *= num_L2;
    leakage_L2         *= num_L2;

    power_nand2_path.readOp.leakage = leak_L1_nand2 * g_tp.peri_global.Vdd;
    power_nand3_path.readOp.leakage = leak_L1_nand3 * g_tp.peri_global.Vdd;
    power_L2.readOp.leakage         = leakage_L2    * g_tp.peri_global.Vdd; 
    area.set_area(cumulative_area_L1 + cumulative_area_L2);
  }
}



pair<double, double> PredecBlk::compute_delays(
    pair<double, double> inrisetime)  // <nand2, nand3>
{
  pair<double, double> ret_val;
  ret_val.first  = 0;  // outrisetime_nand2_path 
  ret_val.second = 0;  // outrisetime_nand3_path

  double inrisetime_nand2_path = inrisetime.first;
  double inrisetime_nand3_path = inrisetime.second;
  int    i;
  double rd, c_load, c_intrinsic, tf, this_delay;
  double Vdd = g_tp.peri_global.Vdd;

  // TODO: following delay calculation part can be greatly simplified.
  // first check whether a predecoder block is required
  if (exist)
  {
    //Find delay in first level of predecoder block
    //First find delay in path 
    if ((flag_two_unique_paths) || (number_inputs_L1_gate == 2))
    {
      //First gate is a NAND2 gate
      rd = tr_R_on(w_L1_nand2_n[0], NCH, 2, is_dram_);
      c_load = gate_C(w_L1_nand2_n[1] + w_L1_nand2_p[1], 0.0, is_dram_);
      c_intrinsic = 2 * drain_C_(w_L1_nand2_p[0], PCH, 1, 1, g_tp.cell_h_def, is_dram_) + 
                        drain_C_(w_L1_nand2_n[0], NCH, 2, 1, g_tp.cell_h_def, is_dram_);
      tf = rd * (c_intrinsic + c_load);
      this_delay = horowitz(inrisetime_nand2_path, tf, 0.5, 0.5, RISE);
      delay_nand2_path += this_delay;
      inrisetime_nand2_path = this_delay / (1.0 - 0.5);
      power_nand2_path.readOp.dynamic += (c_load + c_intrinsic) * Vdd * Vdd;

      //Add delays of all but the last inverter in the chain
      for (i = 1; i < number_gates_L1_nand2_path - 1; ++i)
      {
        rd = tr_R_on(w_L1_nand2_n[i], NCH, 1, is_dram_);
        c_load = gate_C(w_L1_nand2_n[i+1] + w_L1_nand2_p[i+1], 0.0, is_dram_);
        c_intrinsic = drain_C_(w_L1_nand2_p[i], PCH, 1, 1, g_tp.cell_h_def, is_dram_) + 
                      drain_C_(w_L1_nand2_n[i], NCH, 1, 1, g_tp.cell_h_def, is_dram_);
        tf = rd * (c_intrinsic + c_load);
        this_delay = horowitz(inrisetime_nand2_path, tf, 0.5, 0.5, RISE);
        delay_nand2_path += this_delay;
        inrisetime_nand2_path = this_delay / (1.0 - 0.5);
        power_nand2_path.readOp.dynamic += (c_intrinsic + c_load) * Vdd * Vdd;
      }

      //Add delay of the last inverter
      i = number_gates_L1_nand2_path - 1;
      rd = tr_R_on(w_L1_nand2_n[i], NCH, 1, is_dram_);
      if (flag_L2_gate)
      {
        c_load = branch_effort_nand2_gate_output*(gate_C(w_L2_n[0], 0, is_dram_) + gate_C(w_L2_p[0], 0, is_dram_));
        c_intrinsic = drain_C_(w_L1_nand2_p[i], PCH, 1, 1, g_tp.cell_h_def, is_dram_) + 
                      drain_C_(w_L1_nand2_n[i], NCH, 1, 1, g_tp.cell_h_def, is_dram_);
        tf = rd * (c_intrinsic + c_load);
        this_delay = horowitz(inrisetime_nand2_path, tf, 0.5, 0.5, RISE);
        delay_nand2_path += this_delay;
        inrisetime_nand2_path = this_delay / (1.0 - 0.5);
        power_nand2_path.readOp.dynamic += (c_intrinsic + c_load) * Vdd * Vdd;
      }
      else
      { //First level directly drives decoder output load
        c_load = C_ld_predec_blk_out;
        c_intrinsic = drain_C_(w_L1_nand2_p[i], PCH, 1, 1, g_tp.cell_h_def, is_dram_) + 
                      drain_C_(w_L1_nand2_n[i], NCH, 1, 1, g_tp.cell_h_def, is_dram_);
        tf = rd * (c_intrinsic + c_load) + R_wire_predec_blk_out * c_load / 2; 
        this_delay = horowitz(inrisetime_nand2_path, tf, 0.5, 0.5, RISE);
        delay_nand2_path += this_delay;
        ret_val.first = this_delay / (1.0 - 0.5);    
        power_nand2_path.readOp.dynamic += (c_intrinsic + c_load) * Vdd * Vdd;
      }
    }

    if ((flag_two_unique_paths) || (number_inputs_L1_gate == 3))
    { //Check if the number of gates in the first level is more than 1. 
      //First gate is a NAND3 gate
      rd = tr_R_on(w_L1_nand3_n[0], NCH, 3, is_dram_);
      c_load = gate_C(w_L1_nand3_n[1] + w_L1_nand3_p[1], 0.0, is_dram_);
      c_intrinsic = 3 * drain_C_(w_L1_nand3_p[0], PCH, 1, 1, g_tp.cell_h_def, is_dram_) + 
                        drain_C_(w_L1_nand3_n[0], NCH, 3, 1, g_tp.cell_h_def, is_dram_);
      tf = rd * (c_intrinsic + c_load);
      this_delay = horowitz(inrisetime_nand3_path, tf, 0.5, 0.5, RISE);
      delay_nand3_path += this_delay;
      inrisetime_nand3_path = this_delay / (1.0 - 0.5);
      power_nand3_path.readOp.dynamic += (c_intrinsic + c_load) * Vdd * Vdd;

      //Add delays of all but the last inverter in the chain
      for (i = 1; i < number_gates_L1_nand3_path - 1; ++i)
      {
        rd = tr_R_on(w_L1_nand3_n[i], NCH, 1, is_dram_);
        c_load = gate_C(w_L1_nand3_n[i+1] + w_L1_nand3_p[i+1], 0.0, is_dram_);
        c_intrinsic = drain_C_(w_L1_nand3_p[i], PCH, 1, 1, g_tp.cell_h_def, is_dram_) + 
                      drain_C_(w_L1_nand3_n[i], NCH, 1, 1, g_tp.cell_h_def, is_dram_);
        tf = rd * (c_intrinsic + c_load);
        this_delay = horowitz(inrisetime_nand3_path, tf, 0.5, 0.5, RISE);
        delay_nand3_path += this_delay;
        inrisetime_nand3_path = this_delay / (1.0 - 0.5);
        power_nand3_path.readOp.dynamic += (c_intrinsic + c_load) * Vdd * Vdd;
      }

      //Add delay of the last inverter
      i = number_gates_L1_nand3_path - 1;
      rd = tr_R_on(w_L1_nand3_n[i], NCH, 1, is_dram_);
      if (flag_L2_gate) 
      {
        c_load = branch_effort_nand3_gate_output*(gate_C(w_L2_n[0], 0, is_dram_) + gate_C(w_L2_p[0], 0, is_dram_));
        c_intrinsic = drain_C_(w_L1_nand3_p[i], PCH, 1, 1, g_tp.cell_h_def, is_dram_) + 
                      drain_C_(w_L1_nand3_n[i], NCH, 1, 1, g_tp.cell_h_def, is_dram_);
        tf = rd * (c_intrinsic + c_load);
        this_delay = horowitz(inrisetime_nand3_path, tf, 0.5, 0.5, RISE);
        delay_nand3_path += this_delay;
        inrisetime_nand3_path = this_delay / (1.0 - 0.5);
        power_nand3_path.readOp.dynamic += (c_intrinsic + c_load) * Vdd * Vdd;
      }
      else
      { //First level directly drives decoder output load
        c_load = C_ld_predec_blk_out;
        c_intrinsic = drain_C_(w_L1_nand3_p[i], PCH, 1, 1, g_tp.cell_h_def, is_dram_) + 
                      drain_C_(w_L1_nand3_n[i], NCH, 1, 1, g_tp.cell_h_def, is_dram_);
        tf = rd * (c_intrinsic + c_load) + R_wire_predec_blk_out * c_load / 2; 
        this_delay = horowitz(inrisetime_nand3_path, tf, 0.5, 0.5, RISE);
        delay_nand3_path += this_delay;
        ret_val.second = this_delay / (1.0 - 0.5);  
        power_nand3_path.readOp.dynamic += (c_intrinsic + c_load) * Vdd * Vdd;
      }
    }  

    // Find delay through second level 
    if (flag_L2_gate)
    {
      if (flag_L2_gate == 2)
      {
        rd = tr_R_on(w_L2_n[0], NCH, 2, is_dram_);
        c_load = gate_C(w_L2_n[1] + w_L2_p[1], 0.0, is_dram_);
        c_intrinsic = 2 * drain_C_(w_L2_p[0], PCH, 1, 1, g_tp.cell_h_def, is_dram_) + 
                          drain_C_(w_L2_n[0], NCH, 2, 1, g_tp.cell_h_def, is_dram_);
        tf = rd * (c_intrinsic + c_load);
        this_delay = horowitz(inrisetime_nand2_path, tf, 0.5, 0.5, RISE);
        delay_nand2_path += this_delay;
        inrisetime_nand2_path = this_delay / (1.0 - 0.5);
        power_L2.readOp.dynamic += (c_intrinsic + c_load) * Vdd * Vdd;
      }
      else
      { // flag_L2_gate = 3
        rd = tr_R_on(w_L2_n[0], NCH, 3, is_dram_);
        c_load = gate_C(w_L2_n[1] + w_L2_p[1], 0.0, is_dram_);
        c_intrinsic = 3 * drain_C_(w_L2_p[0], PCH, 1, 1, g_tp.cell_h_def, is_dram_) + 
                          drain_C_(w_L2_n[0], NCH, 3, 1, g_tp.cell_h_def, is_dram_);
        tf = rd * (c_intrinsic + c_load);
        this_delay = horowitz(inrisetime_nand3_path, tf, 0.5, 0.5, RISE);
        delay_nand3_path += this_delay;
        inrisetime_nand3_path = this_delay / (1.0 - 0.5);
        power_L2.readOp.dynamic += (c_intrinsic + c_load) * Vdd * Vdd;
      }

      for (i = 1; i < number_gates_L2 - 1; ++i)
      {
        rd = tr_R_on(w_L2_n[i], NCH, 1, is_dram_);
        c_load = gate_C(w_L2_n[i+1] + w_L2_p[i+1], 0.0, is_dram_);
        c_intrinsic = drain_C_(w_L2_p[i], PCH, 1, 1, g_tp.cell_h_def, is_dram_) +
                      drain_C_(w_L2_n[i], NCH, 1, 1, g_tp.cell_h_def, is_dram_);
        tf = rd * (c_intrinsic + c_load);
        this_delay = horowitz(inrisetime_nand2_path, tf, 0.5, 0.5, RISE);
        delay_nand2_path += this_delay;
        inrisetime_nand2_path = this_delay / (1.0 - 0.5);
        this_delay = horowitz(inrisetime_nand3_path, tf, 0.5, 0.5, RISE);
        delay_nand3_path += this_delay;
        inrisetime_nand3_path = this_delay / (1.0 - 0.5);
        power_L2.readOp.dynamic += (c_intrinsic + c_load) * Vdd * Vdd;
      }

      //Add delay of final inverter that drives the wordline decoders
      i = number_gates_L2 - 1;
      c_load = C_ld_predec_blk_out;
      rd = tr_R_on(w_L2_n[i], NCH, 1, is_dram_);
      c_intrinsic = drain_C_(w_L2_p[i], PCH, 1, 1, g_tp.cell_h_def, is_dram_) + 
                    drain_C_(w_L2_n[i], NCH, 1, 1, g_tp.cell_h_def, is_dram_);
      tf = rd * (c_intrinsic + c_load) + R_wire_predec_blk_out * c_load / 2;
      this_delay = horowitz(inrisetime_nand2_path, tf, 0.5, 0.5, RISE);
      delay_nand2_path += this_delay;
      ret_val.first = this_delay / (1.0 - 0.5);  
      this_delay = horowitz(inrisetime_nand3_path, tf, 0.5, 0.5, RISE);
      delay_nand3_path += this_delay;
      ret_val.second = this_delay / (1.0 - 0.5);  
      power_L2.readOp.dynamic += (c_intrinsic + c_load) * Vdd * Vdd;
    }
  }

  delay = (ret_val.first > ret_val.second) ? ret_val.first : ret_val.second;
  return ret_val;
}



PredecBlkDrv::PredecBlkDrv(
    int    way_select_,
    PredecBlk * blk_,
    bool   is_dram)
 :flag_driver_exists(0),
  number_gates_nand2_path(0),
  number_gates_nand3_path(0),
  min_number_gates(2),
  num_buffers_driving_1_nand2_load(0),
  num_buffers_driving_2_nand2_load(0),
  num_buffers_driving_4_nand2_load(0),
  num_buffers_driving_2_nand3_load(0),
  num_buffers_driving_8_nand3_load(0),
  num_buffers_nand3_path(0),
  c_load_nand2_path_out(0),
  c_load_nand3_path_out(0),
  r_load_nand2_path_out(0),
  r_load_nand3_path_out(0),
  delay_nand2_path(0),
  delay_nand3_path(0),
  power_nand2_path(),
  power_nand3_path(),
  blk(blk_), dec(blk->dec),
  is_dram_(is_dram),
  way_select(way_select_)
{
  for (int i = 0; i < MAX_NUMBER_GATES_STAGE; i++)
  {
    width_nand2_path_n[i] = 0;
    width_nand2_path_p[i] = 0;
    width_nand3_path_n[i] = 0;
    width_nand3_path_p[i] = 0;
  }

  number_input_addr_bits = blk->number_input_addr_bits;

  if (way_select > 1)
  {
    flag_driver_exists     = 1;
    number_input_addr_bits = way_select; 
    if (dec->num_in_signals == 2)
    {
      c_load_nand2_path_out = gate_C(dec->w_dec_n[0] + dec->w_dec_p[0], 0, is_dram_);
      num_buffers_driving_2_nand2_load = number_input_addr_bits;
    }
    else if (dec->num_in_signals == 3)
    {
      c_load_nand3_path_out = gate_C(dec->w_dec_n[0] + dec->w_dec_p[0], 0, is_dram_);
      num_buffers_driving_2_nand3_load = number_input_addr_bits;
    }
  }
  else if (way_select == 0)
  {
    if (blk->exist)
    {
      flag_driver_exists = 1;
    }
  }

  compute_widths();
  compute_area();
}



void PredecBlkDrv::compute_widths()
{
  // The predecode block driver accepts as input the address bits from the h-tree network. For 
  // each addr bit it then generates addr and addrbar as outputs. For now ignore the effect of
  // inversion to generate addrbar and simply treat addrbar as addr.

  double F;
  double p_to_n_sz_ratio = pmos_to_nmos_sz_ratio(is_dram_);

  if (flag_driver_exists)
  {
    double C_nand2_gate_blk = gate_C(blk->w_L1_nand2_n[0] + blk->w_L1_nand2_p[0], 0, is_dram_);
    double C_nand3_gate_blk = gate_C(blk->w_L1_nand3_n[0] + blk->w_L1_nand3_p[0], 0, is_dram_);

    if (way_select == 0)
    {
      if (blk->number_input_addr_bits == 1)
      { //2 NAND2 gates
        num_buffers_driving_2_nand2_load = 1;
        c_load_nand2_path_out            = 2 * C_nand2_gate_blk;
      }
      else if (blk->number_input_addr_bits == 2)
      { //4 NAND2 gates
        num_buffers_driving_4_nand2_load = 2;
        c_load_nand2_path_out            = 4 * C_nand2_gate_blk;
      }
      else if (blk->number_input_addr_bits == 3)
      { //8 NAND3 gates
        num_buffers_driving_8_nand3_load = 3;
        c_load_nand3_path_out            = 8 * C_nand3_gate_blk;
      }
      else if (blk->number_input_addr_bits == 4)
      { //4 + 4 NAND2 gates
        num_buffers_driving_4_nand2_load = 4;
        c_load_nand2_path_out            = 4 * C_nand2_gate_blk;
      }
      else if (blk->number_input_addr_bits == 5)
      { //4 NAND2 gates, 8 NAND3 gates
        num_buffers_driving_4_nand2_load = 2;
        num_buffers_driving_8_nand3_load = 3;
        c_load_nand2_path_out            = 4 * C_nand2_gate_blk;
        c_load_nand3_path_out            = 8 * C_nand3_gate_blk;
      }
      else if (blk->number_input_addr_bits == 6)
      { //8 + 8 NAND3 gates
        num_buffers_driving_8_nand3_load = 6;
        c_load_nand3_path_out            = 8 * C_nand3_gate_blk;
      }
      else if (blk->number_input_addr_bits == 7)
      { //4 + 4 NAND2 gates, 8 NAND3 gates
        num_buffers_driving_4_nand2_load = 4;
        num_buffers_driving_8_nand3_load = 3;
        c_load_nand2_path_out            = 4 * C_nand2_gate_blk;
        c_load_nand3_path_out            = 8 * C_nand3_gate_blk;
      }
      else if (blk->number_input_addr_bits == 8)
      { //4 NAND2 gates, 8 + 8 NAND3 gates
        num_buffers_driving_4_nand2_load = 2;
        num_buffers_driving_8_nand3_load = 6;
        c_load_nand2_path_out            = 4 * C_nand2_gate_blk;
        c_load_nand3_path_out            = 8 * C_nand3_gate_blk;
      }
      else if (blk->number_input_addr_bits == 9)
      { //8 + 8 + 8 NAND3 gates
        num_buffers_driving_8_nand3_load = 9;
        c_load_nand3_path_out            = 8 * C_nand3_gate_blk;
      }
    }

    if ((blk->flag_two_unique_paths) || 
        (blk->number_inputs_L1_gate == 2) ||
        (number_input_addr_bits == 0) ||
        ((way_select)&&(dec->num_in_signals == 2)))
    { //this means that way_select is driving NAND2 in decoder. 
      width_nand2_path_n[0] = g_tp.min_w_nmos_;
      width_nand2_path_p[0] = p_to_n_sz_ratio * width_nand2_path_n[0];
      F = c_load_nand2_path_out / gate_C(width_nand2_path_n[0] + width_nand2_path_p[0], 0, is_dram_);
      number_gates_nand2_path = logical_effort(
          min_number_gates,
          1,
          F,
          width_nand2_path_n,
          width_nand2_path_p,
          c_load_nand2_path_out,
          p_to_n_sz_ratio,
          is_dram_, false, g_tp.max_w_nmos_);
    }

    if ((blk->flag_two_unique_paths) ||
        (blk->number_inputs_L1_gate == 3) ||
        ((way_select)&&(dec->num_in_signals == 3)))
    { //this means that way_select is driving NAND3 in decoder. 
      width_nand3_path_n[0] = g_tp.min_w_nmos_;
      width_nand3_path_p[0] = p_to_n_sz_ratio * width_nand3_path_n[0];
      F = c_load_nand3_path_out / gate_C(width_nand3_path_n[0] + width_nand3_path_p[0], 0, is_dram_);
      number_gates_nand3_path = logical_effort(
          min_number_gates,
          1,
          F,
          width_nand3_path_n,
          width_nand3_path_p,
          c_load_nand3_path_out,
          p_to_n_sz_ratio,
          is_dram_, false, g_tp.max_w_nmos_);
    }
  }
}



void PredecBlkDrv::compute_area()
{
  double area_nand2_path = 0;
  double area_nand3_path = 0;
  double leak_nand2_path = 0;
  double leak_nand3_path = 0;

  if (flag_driver_exists)
  { // first check whether a predecoder block driver is needed
    for (int i = 0; i < number_gates_nand2_path; ++i)
    {
      area_nand2_path += compute_gate_area(INV, 1, width_nand2_path_p[i], width_nand2_path_n[i], g_tp.cell_h_def); 
      leak_nand2_path += cmos_Ileak(width_nand2_path_n[i], width_nand2_path_p[i], is_dram_) * INV_LEAK_STACK_FACTOR;
    }
    area_nand2_path *= (num_buffers_driving_1_nand2_load +
                        num_buffers_driving_2_nand2_load +
                        num_buffers_driving_4_nand2_load);
    leak_nand2_path *= (num_buffers_driving_1_nand2_load +
                        num_buffers_driving_2_nand2_load +
                        num_buffers_driving_4_nand2_load);

    for (int i = 0; i < number_gates_nand3_path; ++i)
    {
      area_nand3_path += compute_gate_area(INV, 1, width_nand3_path_p[i], width_nand3_path_n[i], g_tp.cell_h_def); 
      leak_nand3_path += cmos_Ileak(width_nand3_path_n[i], width_nand3_path_p[i], is_dram_) * INV_LEAK_STACK_FACTOR;
    }
    area_nand3_path *= (num_buffers_driving_2_nand3_load + num_buffers_driving_8_nand3_load);
    leak_nand3_path *= (num_buffers_driving_2_nand3_load + num_buffers_driving_8_nand3_load);

    power_nand2_path.readOp.leakage = leak_nand2_path * g_tp.peri_global.Vdd;
    power_nand3_path.readOp.leakage = leak_nand3_path * g_tp.peri_global.Vdd;
    area.set_area(area_nand2_path + area_nand3_path);
  }    
}



pair<double, double> PredecBlkDrv::compute_delays(
    double inrisetime_nand2_path,
    double inrisetime_nand3_path)
{
  pair<double, double> ret_val;
  ret_val.first  = 0;  // outrisetime_nand2_path 
  ret_val.second = 0;  // outrisetime_nand3_path
  int i;
  double rd, c_gate_load, c_load, c_intrinsic, tf, this_delay;
  double Vdd = g_tp.peri_global.Vdd;

  if (flag_driver_exists)
  {
    for (i = 0; i < number_gates_nand2_path - 1; ++i)
    {
      rd = tr_R_on(width_nand2_path_n[i], NCH, 1, is_dram_);
      c_gate_load = gate_C(width_nand2_path_p[i+1] + width_nand2_path_n[i+1], 0.0, is_dram_);
      c_intrinsic = drain_C_(width_nand2_path_p[i], PCH, 1, 1, g_tp.cell_h_def, is_dram_) +
                    drain_C_(width_nand2_path_n[i], NCH, 1, 1, g_tp.cell_h_def, is_dram_);
      tf = rd * (c_intrinsic + c_gate_load);
      this_delay = horowitz(inrisetime_nand2_path, tf, 0.5, 0.5, RISE);
      delay_nand2_path += this_delay;
      inrisetime_nand2_path = this_delay / (1.0 - 0.5);
      power_nand2_path.readOp.dynamic += (c_gate_load + c_intrinsic) * 0.5 * Vdd * Vdd;
    }

    // Final inverter drives the predecoder block or the decoder output load 
    if (number_gates_nand2_path != 0)
    {
      i = number_gates_nand2_path - 1;
      rd = tr_R_on(width_nand2_path_n[i], NCH, 1, is_dram_);
      c_intrinsic = drain_C_(width_nand2_path_p[i], PCH, 1, 1, g_tp.cell_h_def, is_dram_) + 
                    drain_C_(width_nand2_path_n[i], NCH, 1, 1, g_tp.cell_h_def, is_dram_);
      c_load = c_load_nand2_path_out;
      tf = rd * (c_intrinsic + c_load) + r_load_nand2_path_out*c_load/ 2;
      this_delay = horowitz(inrisetime_nand2_path, tf, 0.5, 0.5, RISE);
      delay_nand2_path += this_delay;
      ret_val.first = this_delay / (1.0 - 0.5);  
      power_nand2_path.readOp.dynamic += (c_intrinsic + c_load) * 0.5 * Vdd * Vdd;
    }

    for (i = 0; i < number_gates_nand3_path - 1; ++i)
    {
      rd = tr_R_on(width_nand3_path_n[i], NCH, 1, is_dram_);
      c_gate_load = gate_C(width_nand3_path_p[i+1] + width_nand3_path_n[i+1], 0.0, is_dram_);
      c_intrinsic = drain_C_(width_nand3_path_p[i], PCH, 1, 1, g_tp.cell_h_def, is_dram_) + 
                    drain_C_(width_nand3_path_n[i], NCH, 1, 1, g_tp.cell_h_def, is_dram_);
      tf = rd * (c_intrinsic + c_gate_load);
      this_delay = horowitz(inrisetime_nand3_path, tf, 0.5, 0.5, RISE);
      delay_nand3_path += this_delay;
      inrisetime_nand3_path = this_delay / (1.0 - 0.5);
      power_nand3_path.readOp.dynamic += (c_gate_load + c_intrinsic) * 0.5 * Vdd * Vdd;
    }

    // Final inverter drives the predecoder block or the decoder output load 
    if (number_gates_nand3_path != 0)
    {
      i = number_gates_nand3_path - 1;
      rd = tr_R_on(width_nand3_path_n[i], NCH, 1, is_dram_);
      c_intrinsic = drain_C_(width_nand3_path_p[i], PCH, 1, 1, g_tp.cell_h_def, is_dram_) + 
                    drain_C_(width_nand3_path_n[i], NCH, 1, 1, g_tp.cell_h_def, is_dram_);
      c_load = c_load_nand3_path_out;
      tf = rd*(c_intrinsic + c_load) + r_load_nand3_path_out*c_load / 2;
      this_delay = horowitz(inrisetime_nand3_path, tf, 0.5, 0.5, RISE);
      delay_nand3_path += this_delay;
      ret_val.second = this_delay / (1.0 - 0.5);  
      power_nand3_path.readOp.dynamic += (c_intrinsic + c_load) * 0.5 * Vdd * Vdd;
    }
  }
  return ret_val;
}


double PredecBlkDrv::get_rdOp_dynamic_E(int num_act_mats_hor_dir)
{
  return (num_addr_bits_nand2_path()*power_nand2_path.readOp.dynamic +
          num_addr_bits_nand3_path()*power_nand3_path.readOp.dynamic) * num_act_mats_hor_dir;
}



Predec::Predec(
    PredecBlkDrv * drv1_,
    PredecBlkDrv * drv2_)
:blk1(drv1_->blk), blk2(drv2_->blk), drv1(drv1_), drv2(drv2_)
{
  driver_power.readOp.leakage = drv1->power_nand2_path.readOp.leakage +
                                drv1->power_nand3_path.readOp.leakage +
                                drv2->power_nand2_path.readOp.leakage +
                                drv2->power_nand3_path.readOp.leakage;
  block_power.readOp.leakage = blk1->power_nand2_path.readOp.leakage +
                               blk1->power_nand3_path.readOp.leakage +
                               blk1->power_L2.readOp.leakage +
                               blk2->power_nand2_path.readOp.leakage +
                               blk2->power_nand3_path.readOp.leakage +
                               blk2->power_L2.readOp.leakage;
  power.readOp.leakage = driver_power.readOp.leakage + block_power.readOp.leakage;
}



double Predec::compute_delays(double inrisetime)
{
  // TODO: predecoder block driver should be between decoder and predecoder block.
  pair<double, double> tmp_pair1, tmp_pair2;
  tmp_pair1 = drv1->compute_delays(inrisetime, inrisetime);
  tmp_pair1 = blk1->compute_delays(tmp_pair1);
  tmp_pair2 = drv2->compute_delays(inrisetime, inrisetime);
  tmp_pair2 = blk2->compute_delays(tmp_pair2);
  tmp_pair1 = get_max_delay_before_decoder(tmp_pair1, tmp_pair2);

  driver_power.readOp.dynamic =
    drv1->num_addr_bits_nand2_path() * drv1->power_nand2_path.readOp.dynamic +
    drv1->num_addr_bits_nand3_path() * drv1->power_nand3_path.readOp.dynamic +
    drv2->num_addr_bits_nand2_path() * drv2->power_nand2_path.readOp.dynamic +
    drv2->num_addr_bits_nand3_path() * drv2->power_nand3_path.readOp.dynamic;

  block_power.readOp.dynamic = 
    blk1->power_nand2_path.readOp.dynamic +
    blk1->power_nand3_path.readOp.dynamic +
    blk1->power_L2.readOp.dynamic +
    blk2->power_nand2_path.readOp.dynamic +
    blk2->power_nand3_path.readOp.dynamic +
    blk2->power_L2.readOp.dynamic;

  power.readOp.dynamic = driver_power.readOp.dynamic + block_power.readOp.dynamic;

  delay = tmp_pair1.first;
  return  tmp_pair1.second;
}



// returns <delay, risetime>
pair<double, double> Predec::get_max_delay_before_decoder(
    pair<double, double> input_pair1,
    pair<double, double> input_pair2)
{
  pair<double, double> ret_val;
  double delay;

  delay = drv1->delay_nand2_path + blk1->delay_nand2_path;
  ret_val.first  = delay;
  ret_val.second = input_pair1.first;
  delay = drv1->delay_nand3_path + blk1->delay_nand3_path;
  if (ret_val.first < delay)
  {
    ret_val.first  = delay;
    ret_val.second = input_pair1.second;
  }
  delay = drv2->delay_nand2_path + blk2->delay_nand2_path;
  if (ret_val.first < delay)
  {
    ret_val.first  = delay;
    ret_val.second = input_pair2.first;
  }
  delay = drv2->delay_nand3_path + blk2->delay_nand3_path;
  if (ret_val.first < delay)
  {
    ret_val.first  = delay;
    ret_val.second = input_pair2.second;
  }

  return ret_val;
}



Driver::Driver(double c_gate_load_, double c_wire_load_, double r_wire_load_, bool is_dram)
:number_gates(0),
  min_number_gates(2),
  c_gate_load(c_gate_load_),
  c_wire_load(c_wire_load_),
  r_wire_load(r_wire_load_),
  delay(0),
  power(),
  is_dram_(is_dram)
{
  for (int i = 0; i < MAX_NUMBER_GATES_STAGE; i++)
  {
    width_n[i] = 0;
    width_p[i] = 0;
  }

  compute_widths();
}


void Driver::compute_widths()
{
  double p_to_n_sz_ratio = pmos_to_nmos_sz_ratio(is_dram_);
  double c_load = c_gate_load + c_wire_load;
  width_n[0] = g_tp.min_w_nmos_;
  width_p[0] = p_to_n_sz_ratio * g_tp.min_w_nmos_;

  double F = c_load / gate_C(width_n[0] + width_p[0], 0, is_dram_);
  number_gates = logical_effort(
      min_number_gates,
      1,
      F,
      width_n,
      width_p,
      c_load,
      p_to_n_sz_ratio,
      is_dram_, false,
      g_tp.max_w_nmos_);
}



double Driver::compute_delay(double inrisetime)
{
  int    i;
  double rd, c_load, c_intrinsic, tf;
  double this_delay = 0;

  for (i = 0; i < number_gates - 1; ++i)
  {
    rd = tr_R_on(width_n[i], NCH, 1, is_dram_);
    c_load = gate_C(width_n[i+1] + width_p[i+1], 0.0, is_dram_);
    c_intrinsic = drain_C_(width_p[i], PCH, 1, 1, g_tp.cell_h_def, is_dram_) +  
                  drain_C_(width_n[i], NCH, 1, 1, g_tp.cell_h_def, is_dram_);
    tf = rd * (c_intrinsic + c_load);
    this_delay = horowitz(inrisetime, tf, 0.5, 0.5, RISE);
    delay += this_delay;
    inrisetime = this_delay / (1.0 - 0.5);
    power.readOp.dynamic += (c_intrinsic + c_load) * g_tp.peri_global.Vdd * g_tp.peri_global.Vdd;
    power.readOp.leakage += cmos_Ileak(width_n[i], width_p[i], is_dram_) *
      0.5 * g_tp.peri_global.Vdd;
  }

  i = number_gates - 1;
  c_load = c_gate_load + c_wire_load;
  rd = tr_R_on(width_n[i], NCH, 1, is_dram_);
  c_intrinsic = drain_C_(width_p[i], PCH, 1, 1, g_tp.cell_h_def, is_dram_) +  
                drain_C_(width_n[i], NCH, 1, 1, g_tp.cell_h_def, is_dram_);
  tf = rd * (c_intrinsic + c_load) + r_wire_load * (c_wire_load / 2 + c_gate_load);
  this_delay = horowitz(inrisetime, tf, 0.5, 0.5, RISE);
  delay += this_delay;
  power.readOp.dynamic += (c_intrinsic + c_load) * g_tp.peri_global.Vdd * g_tp.peri_global.Vdd;
  power.readOp.leakage += cmos_Ileak(width_n[i], width_p[i], is_dram_) *
    0.5 * g_tp.peri_global.Vdd;

  return this_delay / (1.0 - 0.5);
}

