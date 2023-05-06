#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WRenoTest");

const uint16_t SINK_PORT = 12450;
const uint32_t SEGMENT_SIZE = 1000;

//Global CMD args
double globalStartTime = 1.0;
double globalStopTime = 100.0;
uint32_t rwndSize = 100000000;
uint32_t traceNode = 0x7fffffff;
std::string tcpSocketType = "TcpNewReno";
bool enCwndTrace = false;
bool enRwndTrace = false;
bool enAwndTrace = false;
bool enBytesInFlightTrace = false;
bool enQueueTrace = false;
bool enRttTrace = false;
bool enRawRttTrace = false;

NodeContainer nodes;

std::map<Ipv4Address, uint32_t> addrMap;

std::map<uint32_t, uint32_t> queuePacketsCounter;
uint32_t queuePackets = 0;

//Trace output stream
std::ofstream rxOutput;
std::ofstream cwndOutput;
std::ofstream rwndOutput;
std::ofstream awndOutput;
std::ofstream bifOutput;
std::ofstream queueOutput;
std::ofstream rttOutput;
std::ofstream rawRttOutput;

void
EnqueueTrace (Ptr<const QueueDiscItem> packet)
{
  NS_LOG_FUNCTION (packet);

  //Resolve Ipv4 Header
  const Ipv4QueueDiscItem* iqdt = dynamic_cast<const Ipv4QueueDiscItem*> (GetPointer(packet));
  NS_ASSERT (iqdt);
  Ipv4Header iph = iqdt->GetHeader();
  Ipv4Address srcIp = iph.GetSource();

  //Map ip addr to flow id
  if (addrMap.find(srcIp) != addrMap.end())
  {
    uint32_t flowId = addrMap[srcIp];
    queuePacketsCounter[flowId] += 1;
    queuePackets += 1;
    
    queueOutput << Simulator::Now ().GetSeconds () << ":";
    for (uint i = 0; i < queuePacketsCounter.size(); ++i)
    {
      queueOutput << queuePacketsCounter[i] / (double)queuePackets << " ";
    }
    queueOutput << std::endl;
  }
}

void
DequeueTrace (Ptr<const QueueDiscItem> packet)
{
  NS_LOG_FUNCTION (packet);

  //Resolve Ipv4 Header
  const Ipv4QueueDiscItem* iqdt = dynamic_cast<const Ipv4QueueDiscItem*> (GetPointer(packet));
  NS_ASSERT (iqdt);
  Ipv4Header iph = iqdt->GetHeader();
  Ipv4Address srcIp = iph.GetSource();

  //Map ip addr to flow id
  if (addrMap.find(srcIp) != addrMap.end())
  {
    uint32_t flowId = addrMap[srcIp];
    queuePacketsCounter[flowId] -= 1;
    queuePackets -= 1;
    if (queuePackets == 0) return;

    queueOutput << Simulator::Now ().GetSeconds () << ":";
    for (uint i = 0; i < queuePacketsCounter.size(); ++i)
    {
      queueOutput << queuePacketsCounter[i] / (double)queuePackets << " ";
    }
    queueOutput << std::endl;
  }
}

void
CwndTrace (uint32_t flowId, uint32_t oldValue, uint32_t newValue)
{
  NS_LOG_FUNCTION (flowId << oldValue << newValue);

  cwndOutput << Simulator::Now ().GetSeconds () << ","
	           << flowId << ","
	           << newValue << std::endl;
}

void
RwndTrace (uint32_t flowId, uint32_t oldValue, uint32_t newValue)
{
  NS_LOG_FUNCTION (flowId << oldValue << newValue);

  rwndOutput << Simulator::Now ().GetSeconds () << ","
	           << flowId << ","
	           << newValue << std::endl;
}

void
AwndTrace (uint32_t flowId, uint32_t oldValue, uint32_t newValue)
{
  NS_LOG_FUNCTION (flowId << oldValue << newValue);

  awndOutput << Simulator::Now ().GetSeconds () << ","
	           << flowId << ","
	           << newValue << std::endl;
}

void
BytesInFlightTrace (uint32_t flowId, uint32_t oldValue, uint32_t newValue)
{
  NS_LOG_FUNCTION (flowId << oldValue << newValue);

  bifOutput << Simulator::Now ().GetSeconds () << ","
	           << flowId << ","
	           << newValue << std::endl;
}

void 
RttTrace (uint32_t flowId, Time oldValue, Time newValue)
{
  NS_LOG_FUNCTION (flowId << oldValue << newValue);

  rttOutput << Simulator::Now ().GetSeconds () << ","
	    << flowId << ","
	    << newValue.GetMicroSeconds () << std::endl;
}

void 
RawRttTrace (uint32_t flowId, Time oldValue, Time newValue)
{
  NS_LOG_FUNCTION (flowId << oldValue << newValue);

  rawRttOutput << Simulator::Now ().GetSeconds () << ","
	    << flowId << ","
	    << newValue.GetMicroSeconds () << std::endl;
}

void
TxTrace (double weight, uint32_t flowId, Ptr<const Packet> packet, const TcpHeader& header, Ptr<const TcpSocketBase> socket)
{
  NS_LOG_FUNCTION (weight << packet);

  FlowWeightTag wTag;
  wTag.SetWeight (weight);
  FlowIdTag idTag;
  idTag.SetFlowId (flowId);

  packet->AddPacketTag (wTag);
  packet->AddPacketTag (idTag);
}

void
RxTrace (Ptr<const Packet> packet, const TcpHeader& header, Ptr<const TcpSocketBase> socket)
{
  NS_LOG_FUNCTION (packet << header << socket);

  FlowIdTag idTag;
  packet->PeekPacketTag (idTag);
  
  rxOutput << Simulator::Now ().GetSeconds () << ","
           << idTag.GetFlowId () << ","
           << packet->GetSize () << std::endl;
}

void
RxSocketCreateTrace (Ptr<Socket> socket)
{
  socket->TraceConnectWithoutContext ("Rx", MakeCallback (RxTrace));
}

void
TxSocketCreateTrace (double weight, uint32_t flowId, Ptr<Socket> socket)
{
  socket->SetAttribute ("FlowWeight", DoubleValue(weight));

  socket->TraceConnectWithoutContext ("Tx", MakeBoundCallback (TxTrace, weight, flowId));

  if (enCwndTrace)
    socket->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback (CwndTrace, flowId));

  if (enRwndTrace)
    socket->TraceConnectWithoutContext ("RWND", MakeBoundCallback (RwndTrace, flowId));

  if (enAwndTrace)
    socket->TraceConnectWithoutContext ("AdvWND", MakeBoundCallback (AwndTrace, flowId));

  if (enBytesInFlightTrace)
    socket->TraceConnectWithoutContext ("BytesInFlight", MakeBoundCallback (BytesInFlightTrace, flowId));

  if (enRttTrace)
    socket->TraceConnectWithoutContext ("RTT", MakeBoundCallback (RttTrace, flowId));

  if (enRawRttTrace)
    socket->TraceConnectWithoutContext ("RawRTT", MakeBoundCallback (RawRttTrace, flowId));

  NS_LOG_UNCOND ("Create Tx Socket ----" << " Type: " << tcpSocketType << " Weight: " << weight << " Flow Id: " << flowId);
}

void
SetupTopology (const std::string &file)
{
  NS_LOG_FUNCTION (file);

  std::ifstream fin (file);
  if (fin.fail ())
    {
      NS_LOG_INFO ("Cannot open topo file: " << file);
      exit (0);
    }
  uint32_t nodeNum, linkNum;

  fin >> nodeNum >> linkNum;

  NS_LOG_INFO ("Create nodes");
  nodes.Create (nodeNum);

  NS_LOG_INFO ("Install internet stack on all nodes");
  InternetStackHelper internet;
  if (tcpSocketType == "NewReno")
    internet.SetTcp(ns3::TcpL4Protocol::GetTypeId().GetName(), "SocketType", TypeIdValue(ns3::TcpNewReno::GetTypeId()));
  else if (tcpSocketType == "Multcp")
    internet.SetTcp(ns3::TcpL4Protocol::GetTypeId().GetName(), "SocketType", TypeIdValue(ns3::TcpMultcp::GetTypeId()));
  else if (tcpSocketType == "Ewtcp")
    internet.SetTcp(ns3::TcpL4Protocol::GetTypeId().GetName(), "SocketType", TypeIdValue(ns3::TcpEwtcp::GetTypeId()));
  else if (tcpSocketType == "WrenoAI")
    internet.SetTcp(ns3::TcpL4Protocol::GetTypeId().GetName(), "SocketType", TypeIdValue(ns3::TcpWrenoAI::GetTypeId()));
  else if (tcpSocketType == "WrenoMD")
    internet.SetTcp(ns3::TcpL4Protocol::GetTypeId().GetName(), "SocketType", TypeIdValue(ns3::TcpWrenoMD::GetTypeId()));
  else
  {
    NS_LOG_INFO ("Undefined Tcp Socket Type");
    NS_ASSERT(0);
  }
  internet.Install (nodes);

  Ipv4AddressHelper ipv4 ("10.0.0.0", "255.255.255.0");

  NS_LOG_INFO ("Create channels");
  PointToPointHelper p2p;
  p2p.SetQueue ("ns3::DropTailQueue");
  for (uint32_t i = 0; i < linkNum; ++i)
    {
      uint32_t src, dst, qdiscSize, threshold;
      std::string dataRate, linkDelay, qdiscType;
      fin >> src >> dst >> dataRate >> linkDelay >> qdiscType >> qdiscSize >> threshold;

      p2p.SetDeviceAttribute ("DataRate", StringValue (dataRate));
      p2p.SetChannelAttribute ("Delay", StringValue (linkDelay));

      NetDeviceContainer devices = p2p.Install (nodes.Get (src), nodes.Get (dst));
      /// queue disc should be added before IP address is assigned
      NS_LOG_INFO ("Install queue disc");
      if (qdiscType == "PfifoFast")
        {
          //install host queue
          TrafficControlHelper tch1;
          tch1.SetRootQueueDisc ("ns3::PfifoFastQueueDisc", "MaxSize", QueueSizeValue (QueueSize(ns3::QueueSizeUnit::PACKETS, qdiscSize)));
          QueueDiscContainer hostQueue = tch1.Install (devices.Get (0));

          //install switch queue
          TrafficControlHelper tch2;
          tch2.SetRootQueueDisc ("ns3::PfifoFastQueueDisc", "MaxSize", QueueSizeValue (QueueSize(ns3::QueueSizeUnit::PACKETS, qdiscSize)));
          QueueDiscContainer switchQueue = tch2.Install (devices.Get (1));

          if (src == traceNode && enQueueTrace)
          {//trace the switch queue which face to the trace node
            switchQueue.Get(0)->TraceConnectWithoutContext ("Enqueue", MakeCallback (EnqueueTrace));
            switchQueue.Get(0)->TraceConnectWithoutContext ("Dequeue", MakeCallback (DequeueTrace));
          }
        }
      else if (qdiscType == "Red")
        {
          //install host queue
          TrafficControlHelper tch1;
          tch1.SetRootQueueDisc ("ns3::RedQueueDisc", "MaxSize", QueueSizeValue (QueueSize(ns3::QueueSizeUnit::PACKETS, qdiscSize)));
          QueueDiscContainer hostQueue = tch1.Install (devices.Get (0));

          //install switch queue
          TrafficControlHelper tch2;
          tch2.SetRootQueueDisc ("ns3::RedQueueDisc", "MaxSize", QueueSizeValue (QueueSize(ns3::QueueSizeUnit::PACKETS, qdiscSize)));
          QueueDiscContainer switchQueue = tch2.Install (devices.Get (1));

          if (src == traceNode && enQueueTrace)
          {//trace the switch queue which face to the trace node
            switchQueue.Get(0)->TraceConnectWithoutContext ("Enqueue", MakeCallback (EnqueueTrace));
            switchQueue.Get(0)->TraceConnectWithoutContext ("Dequeue", MakeCallback (DequeueTrace));
          }
        }
      else
        {
          NS_ABORT_MSG ("qdiscType: " << qdiscType << " not valid");
        }

      NS_LOG_INFO ("Assign IP address");
      Ipv4InterfaceContainer ipv4AddrPool = ipv4.Assign (devices);
      NS_LOG_INFO ("src: " << ipv4AddrPool.GetAddress(0, 0) << " dst: " << ipv4AddrPool.GetAddress(1, 0));
      ipv4.NewNetwork ();
    }
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  fin.close ();
}

void
SetupApp (const std::string &file)
{
  NS_LOG_FUNCTION (file);

  std::set<uint32_t> serverSet;	// nodes include sender and receiver
  std::set<uint32_t> sinkSet;	// nodes include receiver

  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), SINK_PORT));

  std::ifstream fin (file);
  if (fin.fail ())
    {
      NS_LOG_INFO ("Cannot open application file: " << file);
      exit (0);
    }
  uint32_t flowNum;
  fin >> flowNum;
  for (uint32_t i = 0; i < flowNum; ++i)
    {
      uint32_t src, dst, sendSize;
      uint64_t maxBytes;
      double startTime, stopTime;
      double weight;
      fin >> src >> dst >> sendSize >> maxBytes
          >> startTime >> stopTime >> weight;
      NS_ASSERT (startTime >= globalStartTime);
      
      if (serverSet.find (src) == serverSet.end ())
        {
          serverSet.insert (src);
        }
      if (serverSet.find (dst) == serverSet.end ())
        {
          serverSet.insert (dst);
        }

      // install packetSink if no sink is intalled on dst
      if (sinkSet.find (dst) == sinkSet.end	())
        {
          auto sinks = sinkHelper.Install (nodes.Get (dst));
          sinks.Start (Seconds (globalStartTime));
          sinks.Stop (Seconds (globalStopTime));
          // add trace source
          sinks.Get (0)->TraceConnectWithoutContext ("SocketCreate", MakeCallback (RxSocketCreateTrace));
          sinkSet.insert (dst);
        }

      // install sender
      InetSocketAddress sinkAddr (nodes.Get (dst)->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal (), SINK_PORT);
      ApplicationContainer sender;
      BulkSendHelper bulkHelper ("ns3::TcpSocketFactory", sinkAddr);
      sender = bulkHelper.Install (nodes.Get (src));
      sender.Start (Seconds (startTime));
      sender.Stop (Seconds (stopTime));
      NS_LOG_INFO ("Flow " << i << " starts @ " << startTime << " ends @ " << stopTime);

      sender.Get (0)->TraceConnectWithoutContext ("SocketCreate", MakeBoundCallback (TxSocketCreateTrace, weight, i));

      queuePacketsCounter[i] = 0;
    }

  fin.close ();
}

int
main (int argc, char *argv[])
{
  std::string topologyFile;
  std::string flowFile;
  std::string tracePath;

  // TCP params
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (SEGMENT_SIZE));
  Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1));
  Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue (10));
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (rwndSize));
  Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (rwndSize));
  Config::SetDefault ("ns3::TcpSocketBase::MinRto", TimeValue (MilliSeconds (10)));
  Config::SetDefault ("ns3::TcpSocketBase::Sack", BooleanValue (true));
  Config::SetDefault ("ns3::RttMeanDeviation::Alpha", DoubleValue (0.12));
  Config::SetDefault ("ns3::RedQueueDisc::UseHardDrop", BooleanValue (true));
  Config::SetDefault ("ns3::RedQueueDisc::UseEcn", BooleanValue (true));
  Config::SetDefault ("ns3::RedQueueDisc::MinTh", DoubleValue (100));
  Config::SetDefault ("ns3::RedQueueDisc::MaxTh", DoubleValue (500));

  // default config can be overrided by cmd
  CommandLine cmd;
  cmd.AddValue ("topologyFile", "Path to topology file", topologyFile);
  cmd.AddValue ("flowFile", "Path to flow file", flowFile);
  cmd.AddValue ("tracePath", "Path to trace dir", tracePath);
  cmd.AddValue ("startTime", "Global start time", globalStartTime);
  cmd.AddValue ("stopTime", "Global stop time", globalStopTime);
  cmd.AddValue ("defaultRWNDSize", "The default value of max TcpRxBuffer size", rwndSize);
  cmd.AddValue ("tcpSocketType", "The type of tcp socket", tcpSocketType);
  cmd.AddValue ("queueTraceNode", "The destination node", traceNode);
  cmd.AddValue ("enCwndTrace", "Enable Cwnd Trace", enCwndTrace);
  cmd.AddValue ("enRwndTrace", "Enable Rwnd Trace", enRwndTrace);
  cmd.AddValue ("enAwndTrace", "Enable Adv wnd Trace", enAwndTrace);
  cmd.AddValue ("enBytesInFlightTrace", "Enable bytes in flight Trace", enBytesInFlightTrace);
  cmd.AddValue ("enQueueTrace", "Enable Queue Trace", enQueueTrace);
  cmd.AddValue ("enRttTrace", "Enable Rtt Trace", enRttTrace);
  cmd.AddValue ("enRawRttTrace", "Enable Raw Rtt Trace", enRawRttTrace);
  cmd.Parse (argc, argv);

  SetupTopology (topologyFile);
  SetupApp (flowFile);

  rxOutput.open (tracePath + "/rx-trace.txt");
  if (rxOutput.fail())
  {
    NS_LOG_INFO ("Cannot open Rx trace file");
    return 0;
  }
  if (enCwndTrace)
    cwndOutput.open (tracePath + "/cwnd-trace.txt");
  if (enRwndTrace)
    rwndOutput.open (tracePath + "/rwnd-trace.txt");
  if (enAwndTrace)
    awndOutput.open (tracePath + "/awnd-trace.txt");
  if (enBytesInFlightTrace)
    bifOutput.open (tracePath + "/bif-trace.txt");
  if (enQueueTrace)
    queueOutput.open (tracePath + "/queue-trace.txt");
  if (enRttTrace)
    rttOutput.open (tracePath + "/rtt-trace.txt");
  if (enRawRttTrace)
    rawRttOutput.open (tracePath + "/raw-rtt-trace.txt");

  Config::SetDefault ("ns3::ConfigStore::Filename", StringValue (tracePath + "/config.txt"));
  Config::SetDefault ("ns3::ConfigStore::FileFormat", StringValue ("RawText"));
  Config::SetDefault ("ns3::ConfigStore::Mode", StringValue ("Save"));
  ConfigStore outputConfig;
  outputConfig.ConfigureDefaults ();

  NS_LOG_INFO ("Simulation Begin");
  Simulator::Stop (Seconds (globalStopTime));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Simulation End");

  rxOutput.close ();
  return 0;
}
