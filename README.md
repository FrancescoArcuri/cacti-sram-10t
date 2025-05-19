# CACTI – Extension for 10T SRAM Cells

**CACTI** (Computer Architecture Cache Tool Interface) is a tool originally developed by HP Labs for estimating the area, delay, and power consumption of cache and SRAM memory structures. It is widely used in research and development to model the behavior of memory architectures based on various technological and architectural parameters.

This project builds upon **CACTI version 6.5** and extends the original source code to support **10T (10-transistor) SRAM cell architectures**. The modifications allow the tool to accurately model and evaluate the performance, area, and energy characteristics of 10T SRAM cells, which offer improved read/write stability and separate read/write paths compared to traditional 6T and 8T cells.

The source code has been reorganized and extended to integrate these new models, while preserving compatibility with CACTI's existing functionalities.

## Main Code Modifications

The CACTI source code was extended to support **10T SRAM cells** and updated to improve the modeling of **8T cells**. The changes are organized as follows:

### Extensions for 10T SRAM Cell Support

- In the `init_tech_params()` function, new variables were introduced to define the technology parameters specific to the transistors of the 10T cell.
- In the `compute_widths()`, `compute_area()`, and `compute_delays()` functions (within `decoder.cc`), transistor sizes were estimated for the **address decoder**, **demux**, and **driver** blocks. Area, delay, and energy parameters of these components were computed according to the 10T architecture.
- In the `compute_C()` function (within `subarray.cc`), the **wordline load capacitance** and **bitline capacitance** were calculated, considering the contributions of the 10T cell.
- In the `Mat()` constructor, the area of the mat and subarray was redefined to account for the layout of the 10T cell.
- In the `compute_bitline_delay()` function (within `mat.cc`), the **leakage current** of the 10T cell was calculated and integrated into the delay model.

### Updates for 8T Cells (and Shared Logic with 10T)

- In the `compute_bitline_delay()` and `compute_sa_delay()` functions (within `mat.cc`), the **traditional sense amplifier** was replaced with a **skewed inverter**, as typically used in 8T and 10T SRAM cell architectures.

## Configuration via XML Files

This modified version of CACTI relies on XML-based configuration to define memory technologies, cell architectures, and cache structures. These XML files are organized into dedicated folders based on their function:

### XML Directory Structure

- **Devices/**  
  Contains files related to available transistor technologies. Each entry defines physical and electrical parameters such as threshold voltage, leakage current, and junction capacitance.

- **Cell/**  
  Defines the memory cell architectures, including 6T, 8T, and 10T models. Each XML file describes the cell type and its corresponding design parameters and electrical behavior.

- **Cache Configuration/**  
  Contains the structural and operational settings of the memory or cache, such as cache size, number of banks, number of read/write ports, and access mode.

These XML files work together to fully describe the configuration of the memory subsystem being simulated. The user can modify or extend them to experiment with new technology nodes, custom SRAM cells (like the 10T cell), or alternative cache organizations.

>**Note**: Any changes made to the XML files will directly affect the behavior of the simulator and the results of the performance and power estimations.

## 🚀 Running the Tool

Follow the steps below to configure and run the modified version of CACTI:

1. **Edit the XML files** in the `xmls/` folder to define the desired memory configuration.

2. **Open a terminal** and navigate to the root directory of the project.

3. **Compile the project**:

   ```bash
   make
   ```

   This will generate the folder `obj_opt/` containing the compiled executable.

4. **Copy the `cacti.exe` executable** from the `obj_opt/` folder into the main project directory:

   ```bash
   cp obj_opt/cacti.exe .
   ```

5. **Run the simulator** based on the technology you want to test:

   - For **CMOS**:

     ```bash
     ./cacti -infile xmls/cache_config_cmos.xml > pcacti_detailed_report.txt
     ```

   - For **FinFET**:

     ```bash
     ./cacti -infile xmls/cache_config_finfet.xml > pcacti_detailed_report.txt
     ```

6. **Simulation results** are printed at the end of the output file `pcacti_detailed_report.txt`.

> 📝 **Note**: Compilation (`make`) is required **only** if source code files are modified.  
> If you only change XML configuration files, you can **skip compilation** and start directly from step 5.

> ❗ **Warning**: If the folder `obj_opt/` already exists and you need to recompile, it is recommended to delete it first before repeating the process from step 2.


## 👤 Author

This project was developed as part of a Master's thesis in Computer Science and Engineering.  
Author: Francesco Arcuri  
University: Politecnico di Milano  
Year: 2024
