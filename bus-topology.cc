// bus-topology-attacker.cc
// Bus topology using CSMA (shared medium) + Attacker (jammer/flooder)
// Save as scratch/bus-topology-attacker.cc

#include <cmath>
#include <numeric>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE("BusTopologyAttackerExample");

int
main (int argc, char *argv[])
{
  uint32_t nSensors = 5;
  double simTime = 60.0;
  bool enablePcap = true;
  uint16_t port = 50000;

  CommandLine cmd;
  cmd.AddValue("nSensors", "Number of sensor nodes", nSensors);
  cmd.AddValue("simTime", "Simulation time (s)", simTime);
  cmd.Parse(argc, argv);

  // Create nodes: sensors + attacker + gateway
  // We'll create nSensors + 2 nodes: [0..nSensors-1] sensors, [nSensors] attacker, [nSensors+1] gateway
  NodeContainer busNodes;
  busNodes.Create(nSensors + 2);

  NodeContainer sensors;
  for (uint32_t i = 0; i < nSensors; ++i) sensors.Add(busNodes.Get(i));

  Ptr<Node> attacker = busNodes.Get(nSensors);
  Ptr<Node> gateway = busNodes.Get(nSensors + 1);

  // CSMA channel to model a bus (shared medium)
  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

  // Install CSMA devices on all bus nodes (sensors + attacker + gateway)
  NetDeviceContainer devices = csma.Install(busNodes);

  // Mobility: place nodes linearly along X axis to visualize bus
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  double spacing = 10.0;
  for (uint32_t i = 0; i < nSensors; ++i)
    {
      positionAlloc->Add(Vector(i * spacing, 0.0, 0.0)); // sensors
    }
  // attacker next
  positionAlloc->Add(Vector(nSensors * spacing, 0.0, 0.0));
  // gateway at end
  positionAlloc->Add(Vector((nSensors + 1) * spacing, 0.0, 0.0));

  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(busNodes);

  // Internet stack and addressing
  InternetStackHelper internet;
  internet.Install(busNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  // Determine addresses
  Ipv4Address gatewayAddr = interfaces.GetAddress(nSensors + 1); // gateway interface
  Ipv4Address attackerAddr = interfaces.GetAddress(nSensors);     // attacker interface

  // Gateway app: PacketSink
  Ptr<Node> gatewayNode = gateway;
  PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
                              InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApp = sinkHelper.Install(gatewayNode);
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(simTime + 1.0));

  // Sensor apps: periodic UDP to gateway
  OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(gatewayAddr, port));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=0.1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=5.0]"));
  onoff.SetAttribute("PacketSize", UintegerValue(64));
  onoff.SetAttribute("DataRate", StringValue("8kbps"));

  for (uint32_t i = 0; i < sensors.GetN(); ++i)
    {
      ApplicationContainer app = onoff.Install(sensors.Get(i));
      app.Start(Seconds(1.0 + i * 0.2));
      app.Stop(Seconds(simTime));
    }

  // --- Attacker application (Jammer) ---
  // Jammer: large packets at very high data rate directed to gateway
  // To use Flooder instead, reduce PacketSize and adjust DataRate (e.g., 32 bytes @ 10Mb/s)
  OnOffHelper jammerApp("ns3::UdpSocketFactory", Address());
  jammerApp.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  jammerApp.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  jammerApp.SetAttribute("PacketSize", UintegerValue(1400));   // large packets -> channel saturation
  jammerApp.SetAttribute("DataRate", StringValue("50Mb/s"));   // very high - tune to your machine
  InetSocketAddress jamRemote = InetSocketAddress(gatewayAddr, port);
  jammerApp.SetAttribute("Remote", AddressValue(jamRemote));
  ApplicationContainer jamApp = jammerApp.Install(attacker);
  jamApp.Start(Seconds(5.0));                // attack window start
  jamApp.Stop(Seconds(simTime - 5.0));       // attack window end

  // Optional: If you prefer a flooder instead, comment above block and use:
  /*
  OnOffHelper floodApp("ns3::UdpSocketFactory", Address());
  floodApp.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  floodApp.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  floodApp.SetAttribute("PacketSize", UintegerValue(32));    // small flood packets
  floodApp.SetAttribute("DataRate", StringValue("10Mb/s"));
  InetSocketAddress floodRemote = InetSocketAddress(gatewayAddr, port);
  floodApp.SetAttribute("Remote", AddressValue(floodRemote));
  ApplicationContainer floodApps = floodApp.Install(attacker);
  floodApps.Start(Seconds(8.0));
  floodApps.Stop(Seconds(simTime - 8.0));
  */

  // Optional: enable PCAP on CSMA devices (per-node pcap)
  if (enablePcap)
    {
      for (uint32_t i = 0; i < devices.GetN(); ++i)
        {
          csma.EnablePcap("bus-node-" + std::to_string(i), devices.Get(i));
        }
      // Attacker pcap file will be bus-node-<nSensors>.pcap
    }

  // FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // NetAnim
  AnimationInterface anim("bus-attacker.xml");
  // Mark gateway
  anim.UpdateNodeDescription(gatewayNode, "Gateway");
  anim.UpdateNodeColor(gatewayNode, 255, 0, 0);
  // Mark sensors
  for (uint32_t i = 0; i < sensors.GetN(); ++i)
    {
      anim.UpdateNodeDescription(sensors.Get(i), "Sensor-" + std::to_string(i));
      anim.UpdateNodeColor(sensors.Get(i), 0, 255, 0);
    }
  // Mark attacker
  anim.UpdateNodeDescription(attacker, "Attacker");
  anim.UpdateNodeColor(attacker, 255, 0, 255);

  // Run simulation
  Simulator::Stop(Seconds(simTime + 1.0));
  Simulator::Run();

  // ---- Metrics Collection (AFTER Run) ----
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  uint64_t txPackets = 0, rxPackets = 0, txBytes = 0, rxBytes = 0;
  double totalDelay = 0.0, totalJitter = 0.0;
  std::vector<double> perNodeThroughput;

  for (auto &flow : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
      if (t.destinationPort == port)
        {
          double throughput = (flow.second.rxBytes * 8.0) / simTime;
          perNodeThroughput.push_back(throughput);
          txPackets += flow.second.txPackets;
          rxPackets += flow.second.rxPackets;
          txBytes += flow.second.txBytes;
          rxBytes += flow.second.rxBytes;
          totalDelay += flow.second.delaySum.GetSeconds();
          totalJitter += flow.second.jitterSum.GetSeconds();
        }
    }

  // Compute metrics
  double aggThroughput = (rxBytes * 8.0) / simTime;
  double pdr = (txPackets > 0) ? (double)rxPackets / txPackets : 0.0;
  double avgDelay = (rxPackets > 0) ? totalDelay / rxPackets : 0.0;
  double avgJitter = (rxPackets > 0) ? totalJitter / rxPackets : 0.0;
  double lossRatio = (txPackets > 0) ? (double)(txPackets - rxPackets) / txPackets : 0.0;
  double goodput = aggThroughput;
  double energyConsumption = 0.0; // placeholder
  uint32_t collisions = 0; // placeholder
  uint32_t hopCount = 1; // bus is single hop to gateway
  double fairness = 0.0;

  if (!perNodeThroughput.empty())
    {
      double sum = std::accumulate(perNodeThroughput.begin(), perNodeThroughput.end(), 0.0);
      double sumSq = 0.0;
      for (double t : perNodeThroughput) sumSq += t * t;
      fairness = (sum * sum) / (perNodeThroughput.size() * sumSq);
    }

  // ---- Print Metrics ----
  std::cout << "\n==== Performance Metrics ====\n";
  std::cout << "1. Aggregate Throughput: " << aggThroughput << " bps\n";
  std::cout << "2. Packet Delivery Ratio: " << (pdr * 100.0) << " %\n";
  std::cout << "3. Average End-to-End Delay: " << avgDelay << " s\n";
  std::cout << "4. Average Jitter: " << avgJitter << " s\n";
  std::cout << "5. Packet Loss Ratio: " << (lossRatio * 100.0) << " %\n";
  std::cout << "6. Goodput: " << goodput << " bps\n";
  std::cout << "7. Energy Consumption: " << energyConsumption << " J (placeholder)\n";
  std::cout << "8. Collision Count: " << collisions << " (placeholder)\n";
  std::cout << "9. Average Hop Count: " << hopCount << "\n";
  std::cout << "10. Fairness Index: " << fairness << "\n";

  Simulator::Destroy();
  return 0;
}

