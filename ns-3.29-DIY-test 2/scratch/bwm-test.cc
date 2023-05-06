#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/bandwidth-manager-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("BwmTest");

//Global constants
const uint16_t SINK_PORT = 12450;
const uint32_t SEGMENT_SIZE = 1000;

//Global CMD args
double globalStartTime = 1.0;
double globalStopTime = 10.0;
std::string tcpSocketType = "NewReno";
bool enBwmTest = true;
bool enCAWC = false;
bool enCwndTrace = true;
bool enRttTrace = false;
bool enQDCRateTrace = true;
bool enQDCUsageTrace = true;
bool enTenantActFSTrace = true;
bool enUnitFlowAlcFSTrace = true;
bool enUnitFlowUsageTrace = true;
uint32_t coordinatorNode = -1;

//Global data structure
NodeContainer nodes;
std::set<uint32_t> hostNodeSet;

//Trace output stream
std::ofstream rxOutput;
std::ofstream cwndOutput;
std::ofstream rttOutput;
std::ofstream flowAlcFSOutput;
std::ofstream flowUsageOutput;
std::ofstream tenantActFSOutput;
std::ofstream qdcUsageOutput;
std::ofstream qdcRateOutput;

void 
RttTrace (uint32_t flowId, Time oldValue, Time newValue)
{
  rttOutput << Simulator::Now ().GetSeconds () << ","
	    << flowId << ","
	    << newValue.GetMicroSeconds () << std::endl;
}

void
CwndTrace (uint32_t flowId, uint32_t oldValue, uint32_t newValue)
{

  cwndOutput << Simulator::Now ().GetSeconds () << ","
             << flowId << ","
             << newValue << std::endl;
}

void
TxTrace (uint32_t tenantId, uint32_t flowId, Ptr<const Packet> packet, const TcpHeader& header, Ptr<const TcpSocketBase> socket)
{
  TenantIdTag tidTag;
  tidTag.SetTenantId (tenantId);
  FlowIdTag fidTag;
  fidTag.SetFlowId (flowId);

  packet->AddPacketTag (fidTag);
  packet->AddPacketTag (tidTag);

}

void
RxTrace (Ptr<const Packet> packet, const TcpHeader& header, Ptr<const TcpSocketBase> socket)
{
  TenantIdTag tidTag;
  packet->PeekPacketTag (tidTag);
  FlowIdTag fidTag;
  packet->PeekPacketTag (fidTag);
  
  rxOutput << Simulator::Now ().GetSeconds () << ","
           << tidTag.GetTenantId () << ","
           << fidTag.GetFlowId () << ","
           << packet->GetSize () << std::endl;

}

void
RxSocketCreateTrace (Ptr<Socket> socket)
{
  socket->TraceConnectWithoutContext ("Rx", MakeCallback (RxTrace));
}

void
TxSocketCreateTrace (uint32_t tenantId, uint32_t flowId, Ptr<Socket> socket)
{
  socket->TraceConnectWithoutContext ("Tx", MakeBoundCallback (TxTrace, tenantId, flowId));

  if (enRttTrace)
    socket->TraceConnectWithoutContext ("RTT", MakeBoundCallback (RttTrace, flowId));

  if (enCwndTrace)
    socket->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback (CwndTrace, flowId));
}

void
UnitFlowAlcFSTrace (uint32_t traceId, double oldValue, double newValue)
{
  flowAlcFSOutput << Simulator::Now ().GetSeconds () << ","
                  << traceId << ","
                  << newValue << std::endl;
}

void
UnitFlowUsageTrace (uint32_t traceId, double oldValue, double newValue)
{
  flowUsageOutput << Simulator::Now ().GetSeconds () << ","
                  << traceId << ","
                  << newValue << std::endl;
}

void
TenantActFSTrace (uint32_t tenantId, double oldValue, double newValue)
{
  tenantActFSOutput << Simulator::Now ().GetSeconds () << ","
                    << tenantId << ","
                    << newValue << std::endl;
}

void
QDCRateTrace (uint32_t traceId, DataRate oldValue, DataRate newValue)
{
  qdcRateOutput << Simulator::Now ().GetSeconds () << ","
                << traceId << ","
                << (double)newValue.GetBitRate () << std::endl;
}

void
QDCUsageTrace (uint32_t traceId, double oldValue, double newValue)
{
  qdcUsageOutput << Simulator::Now ().GetSeconds () << ","
                 << traceId << ","
                 << newValue << std::endl;
}

void
UnitFlowCreateTrace (Ptr<UnitFlow> flow)
{
  if (enUnitFlowAlcFSTrace)
    {
      flow->TraceConnectWithoutContext ("AllocatedFairShare", MakeBoundCallback ( UnitFlowAlcFSTrace, flow->GetTraceId ()));
    }
  if (enUnitFlowUsageTrace)
    {
      flow->TraceConnectWithoutContext ("Usage", MakeBoundCallback (UnitFlowUsageTrace, flow->GetTraceId ()));
    }
}

void
TenantCreateTrace (Ptr<Tenant> tenant)
{
  if (enTenantActFSTrace)
    {
      tenant->TraceConnectWithoutContext ("ActualFairShare", MakeBoundCallback (TenantActFSTrace, tenant->GetTenantId ()));
    }
}

void
QueueDiscClassCreateTrace (Ptr<BwmQueueDiscClass> qDiscClass)
{
  if (enQDCRateTrace)
    {
      qDiscClass->TraceConnectWithoutContext ("Rate", MakeBoundCallback (QDCRateTrace, qDiscClass->GetTraceId ()));
    }
  if (enQDCUsageTrace)
    {
      qDiscClass->TraceConnectWithoutContext ("Usage", MakeBoundCallback (QDCUsageTrace, qDiscClass->GetTraceId ()));
    }
}

void
ReadBwmConfig (const std::string &file)
{
  NS_LOG_FUNCTION (file);

  std::ifstream fin (file);
  if (fin.fail ())
    {
      NS_LOG_INFO ("Cannot open config file: " << file);
      exit (0);
    }

  int32_t hostNum = 0;
  fin >> hostNum;
  for (int i = 0; i < hostNum; ++i)
    {
      uint32_t hostNode;
      fin >> hostNode;
      hostNodeSet.insert (hostNode);
    }
}

void
SetupTopology (const std::string &file, const std::string & tenantConfigFile)
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
      NS_LOG_WARN ("Undefined Tcp Socket Type");
      NS_ASSERT(0);
    }
  internet.Install (nodes);

  //initialize bwm application factories
  ObjectFactory coordinatorFactory;
  ObjectFactory agentFactory;
  ObjectFactory qdiscFactory;//not use	by tf
  coordinatorFactory.SetTypeId (BwmCoordinator::GetTypeId ());
  agentFactory.SetTypeId (BwmLocalAgent::GetTypeId ());
  qdiscFactory.SetTypeId (BwmQueueDisc::GetTypeId ());

  //install the central coordinator to the first node in host set by default or by designation
  if (hostNodeSet.find (coordinatorNode) == hostNodeSet.end ())
    {
      coordinatorNode = *hostNodeSet.begin ();
    }
  Ptr<BwmCoordinator> coordinator = coordinatorFactory.Create<BwmCoordinator> ();
  nodes.Get (coordinatorNode)->AddApplication (coordinator);
  coordinator->SetStartTime (Seconds (globalStartTime));
  coordinator->SetStopTime (Seconds (globalStopTime));
  coordinator->SetAttribute("ProgressFactor",DoubleValue(0.15));
  coordinator->TraceConnectWithoutContext ("TenantCreate", MakeCallback (TenantCreateTrace));
  coordinator->TraceConnectWithoutContext ("UnitFlowCreate", MakeCallback (UnitFlowCreateTrace));
  coordinator->InputConfiguration (tenantConfigFile);

  //setup switches and hosts
  Ipv4AddressHelper ipv4 ("10.0.0.0", "255.255.255.0");
  NS_LOG_INFO ("Create channels");
  PointToPointHelper p2p;
  p2p.SetQueue ("ns3::DropTailQueue");
  for (uint32_t i = 0; i < linkNum; ++i)
    {
      uint32_t src, dst, qdiscSize;
      std::string dataRate, linkDelay;
      fin >> src >> dst >> dataRate >> linkDelay >> qdiscSize;

      p2p.SetDeviceAttribute ("DataRate", StringValue (dataRate));
      p2p.SetChannelAttribute ("Delay", StringValue (linkDelay));

      NetDeviceContainer devices = p2p.Install (nodes.Get (src), nodes.Get (dst));
      
      /// queue disc should be added before IP address is assigned
      NS_LOG_INFO ("Install queue disc");
      if (enBwmTest && hostNodeSet.find (src) != hostNodeSet.end ())
        {
          //install host queue
          TrafficControlHelper tch1;
          tch1.SetRootQueueDisc ("ns3::BwmQueueDisc", "MaxSize", QueueSizeValue (QueueSize(ns3::QueueSizeUnit::PACKETS, qdiscSize)));
          // TrafficControlHelper::ClassIdList cid = tch1.AddQueueDiscClasses (handle, 1, "ns3::QueueDiscClass");
          // tch1.AddChildQueueDisc (handle, cid[0], "ns3::BwmQueueDisc");

          QueueDiscContainer hostQueue = tch1.Install (devices.Get(0));
          auto qdisc = DynamicCast<BwmQueueDisc> (hostQueue.Get (0));
          // auto tokenBucket = DynamicCast<TbfQueueDisc> (hostQueue.Get (0));
          // auto qdisc = DynamicCast<BwmQueueDisc> (tokenBucket->GetQueueDiscClass (0)->GetQueueDisc ());

          //set up the local agent for the queue disc
          Ptr<BwmLocalAgent> agent = agentFactory.Create<BwmLocalAgent> ();
          qdisc->SetupLocalAgent (agent);
          qdisc->TraceConnectWithoutContext ("FlowCreate", MakeCallback (QueueDiscClassCreateTrace));
          agent->SetQueueDisc (qdisc);

          //install the agent to the host
          nodes.Get (src)->AddApplication (agent);
          agent->SetStartTime (Seconds (globalStartTime));
          agent->SetStopTime (Seconds (globalStopTime));
          agent->SetCoordinator (coordinator);
          agent->SetHostId (src);
          if (enCAWC)
            {
              //if enable congestion-aware work-conserving mechanism, setup the ipv4 layer
              NS_LOG_INFO ("Setup CAWC for Node " << src);
              agent->SetupCAWC (nodes.Get (src)->GetObject<Ipv4> ());
            }

          //install switch queue
          TrafficControlHelper tch2;
          if (enCAWC)
            {
              //use red queues
              tch2.SetRootQueueDisc ("ns3::RedQueueDisc", "MaxSize", QueueSizeValue (QueueSize(ns3::QueueSizeUnit::PACKETS, qdiscSize)));
            }
          else
            {
              //use pfifo queues
              tch2.SetRootQueueDisc ("ns3::PfifoFastQueueDisc", "MaxSize", QueueSizeValue (QueueSize(ns3::QueueSizeUnit::PACKETS, qdiscSize)));
            }
          QueueDiscContainer switchQueue = tch2.Install (devices.Get (1));
        }
      else
        {
          //install switch queue
          TrafficControlHelper tch1;
          if (enCAWC)
            {
              //use red queues
              tch1.SetRootQueueDisc ("ns3::RedQueueDisc", "MaxSize", QueueSizeValue (QueueSize(ns3::QueueSizeUnit::PACKETS, qdiscSize)));
            }
          else
            {
              //use pfifo queues
              tch1.SetRootQueueDisc ("ns3::PfifoFastQueueDisc", "MaxSize", QueueSizeValue (QueueSize(ns3::QueueSizeUnit::PACKETS, qdiscSize)));
            }
          QueueDiscContainer switchQueue = tch1.Install (devices.Get (0));

          //install switch queue
          TrafficControlHelper tch2;
          if (enCAWC)
            {
              //use red queues
              tch2.SetRootQueueDisc ("ns3::RedQueueDisc", "MaxSize", QueueSizeValue (QueueSize(ns3::QueueSizeUnit::PACKETS, qdiscSize)));
            }
          else
            {
              //use pfifo queues
              tch2.SetRootQueueDisc ("ns3::PfifoFastQueueDisc", "MaxSize", QueueSizeValue (QueueSize(ns3::QueueSizeUnit::PACKETS, qdiscSize)));
            }
          switchQueue = tch2.Install (devices.Get (1));
        }

      NS_LOG_INFO ("Assign IP address");
      Ipv4InterfaceContainer ipv4AddrPool = ipv4.Assign (devices);
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
      uint32_t src, dst;
      double startTime, stopTime;
      uint32_t flowId, tenantId;
      fin >> src >> dst >> startTime >> stopTime >> flowId >> tenantId;
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
      NS_LOG_INFO ("Flow " << flowId << " starts @ " << startTime << " ends @ " << stopTime);

      sender.Get (0)->TraceConnectWithoutContext ("SocketCreate", MakeBoundCallback (TxSocketCreateTrace, tenantId, flowId));
    }

  fin.close ();
}

int
main (int argc, char *argv[])
{
  std::string bwmConfigFile; //format: num /line host1 host2 host3
  std::string tenantConfigFile;  //format: /line1 tenantId /line2 x1,y1 x2,y2 /line3 host1,w1 host2,w2
  std::string topologyFile; //format: nodeNum linkNum /line src dst dataRate linkDelay qdiscSize
  std::string flowFile; //format: flowNum /line src dst startTime stopTime flowId tenantId
  std::string tracePath;

  // TCP params
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (SEGMENT_SIZE));
  Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1));
  Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue (10));
//  Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (1e+7));
//  Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (1e+7));
//  Config::SetDefault ("ns3::TcpSocketBase::MinRto", TimeValue (MilliSeconds (10)));
  Config::SetDefault ("ns3::TcpSocketBase::Sack", BooleanValue (true));
  Config::SetDefault ("ns3::TcpSocketBase::EcnMode", EnumValue (TcpSocketBase::EcnMode_t::ClassicEcn));

  // default config can be overrided by cmd
  CommandLine cmd;
  cmd.AddValue ("topologyFile", "Path to topology file", topologyFile);
  cmd.AddValue ("flowFile", "Path to flow file", flowFile);
  cmd.AddValue ("bwmConfigFile", "Path to bwm configuration file", bwmConfigFile);
  cmd.AddValue ("tenantConfigFile", "Path to global configuration file", tenantConfigFile);
  cmd.AddValue ("tracePath", "Path to trace dir", tracePath);
  cmd.AddValue ("startTime", "Global start time", globalStartTime);
  cmd.AddValue ("stopTime", "Global stop time", globalStopTime);
  cmd.AddValue ("tcpSocketType", "The type of tcp socket", tcpSocketType);
  cmd.AddValue ("enCwndTrace", "Enable Cwnd Trace", enCwndTrace);
  cmd.AddValue ("enRttTrace", "Enable Rtt Trace", enRttTrace);
  cmd.AddValue ("enBwmTest", "Enable Bandwidth Manager Test", enBwmTest);
  cmd.AddValue ("enCAWC", "Enable Congestion Aware Work-Conserving Mechanism", enCAWC);
  cmd.AddValue ("coordinatorNode", "The node equipped with coordinator", coordinatorNode);
  cmd.AddValue ("enQDCRateTrace", "Enable Rtt Trace", enQDCRateTrace);
  cmd.AddValue ("enQDCUsageTrace", "Enable Rtt Trace", enQDCUsageTrace);
  cmd.AddValue ("enTenantActFSTrace", "Enable Rtt Trace", enTenantActFSTrace);
  cmd.AddValue ("enUnitFlowAlcFSTrace", "Enable Rtt Trace", enUnitFlowAlcFSTrace);
  cmd.AddValue ("enUnitFlowUsageTrace", "Enable Rtt Trace", enUnitFlowUsageTrace);
  cmd.Parse (argc, argv);

  ReadBwmConfig (bwmConfigFile);
  SetupTopology (topologyFile, tenantConfigFile);
  SetupApp (flowFile);

  rxOutput.open (tracePath + "/rx-trace.txt");
  if (rxOutput.fail())
    {
      NS_LOG_WARN ("Cannot open Rx trace file via " << tracePath);
      return 0;
    }
  cwndOutput.open (tracePath + "/cwnd-trace.txt");
  rttOutput.open (tracePath + "/rtt-trace.txt");
  flowAlcFSOutput.open (tracePath + "/flow-alc-fs-trace.txt");
  flowUsageOutput.open (tracePath + "/flow-usage-trace.txt");
  tenantActFSOutput.open (tracePath + "/tenant-act-fs-trace.txt");
  qdcUsageOutput.open (tracePath + "/qdc-usage-trace.txt");
  qdcRateOutput.open (tracePath + "/qdc-rate-trace.txt");

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
