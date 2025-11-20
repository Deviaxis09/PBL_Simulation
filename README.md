# PBL_Simulation
ns-3 simulation network with packet flow, tracing, and NetAnim visualization. This repository contains the simulation script, NetAnim XML output, and steps to run and view the animation.
# Bus Topology Simulation with Attacker Scenario (ns-3.45)

This repository contains an ns-3.45 simulation of a **bus topology** network with an **attacker scenario**, along with generated NetAnim XML outputs for visualization. The project includes:

- **bus-topology.cc** – Main simulation script  
- **bus-attacker.xml** – NetAnim animation showing attacker behavior  
- **star.xml** – Additional XML animation (optional)  
- Screenshots & supporting files  

---

##  How to Run the Simulation (ns-3.45)

1. Place `bus-topology.cc` inside the `scratch/` folder of your ns-3.45 installation:

~/ns-allinone-3.45/ns-3.45/scratch/

2. Build ns-3:
```bash
./waf build

    Run the simulation:

    ./waf --run scratch/bus-topology

This will generate trace files, including XML output for NetAnim.
🎥 View Animation in NetAnim

    Open NetAnim:

    ./NetAnim

    Load the XML animation:

        bus-attacker.xml

        or star.xml

