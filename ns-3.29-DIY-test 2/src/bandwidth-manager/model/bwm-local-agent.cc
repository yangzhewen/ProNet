#include "ns3/log.h"
#include "ns3/string.h"
#include "ns3/double.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-header.h"
#include "ns3/socket.h"
#include "ns3/packet.h"
#include "ns3/callback.h"
#include "ns3/flow-id-tag.h"
#include <sstream>
#include <utility>

#include "bwm-coordinator.h"
#include "bwm-local-agent.h"
#include "bwm-queue-disc.h"
#include "bandwidth-function.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BwmLocalAgent");

NS_OBJECT_ENSURE_REGISTERED (BwmLocalAgent);

TypeId
BwmLocalAgent::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::BwmLocalAgent")
    .SetParent<Application> ()
    .SetGroupName ("BandwidthManager")
    .AddConstructor<BwmLocalAgent> ()
    .AddAttribute ("LearningRate",
                   "The learning rate of Distributed Edge Optimization Algorithm",
                   DoubleValue (0.05),
                   MakeDoubleAccessor (&BwmLocalAgent::m_k),
                   MakeDoubleChecker<double> (0, 1))
    .AddAttribute ("ReportCycle",
                   "The cycle of collecting and reporting usage",
                   TimeValue (Time ("5ms")),
                   MakeTimeAccessor (&BwmLocalAgent::m_reportCycle),
                   MakeTimeChecker ())
    .AddAttribute ("TuneCycle",
                   "The cycle of tunning rates",
                   TimeValue (Time ("1ms")),
                   MakeTimeAccessor (&BwmLocalAgent::m_tuneCycle),
                   MakeTimeChecker ())
    .AddAttribute ("FeedbackCycle",
                   "The cycle of periodical feedback",
                   TimeValue (Time ("1ms")),
                   MakeTimeAccessor (&BwmLocalAgent::m_feedbackCycle),
                   MakeTimeChecker ())
    .AddAttribute ("CongestionThreshold",
                   "The congestion threshold used for state decision",
                   DoubleValue (0.2),
                   MakeDoubleAccessor (&BwmLocalAgent::m_congestionThreshold),
                   MakeDoubleChecker<double> (0, 1))
    .AddAttribute ("FeedbackThreshold",
                   "The counter threshold used for sending congestion feedback",
                   UintegerValue (50),
                   MakeUintegerAccessor (&BwmLocalAgent::m_feedbackThreshold),
                   MakeUintegerChecker<uint32_t> ())
  ;
  return tid;
}

BwmLocalAgent::BwmLocalAgent ()
  : m_timer (Timer::CANCEL_ON_DESTROY),
    m_subTimer (Timer::CANCEL_ON_DESTROY),
    m_targetStatus (0),
    m_deviceRateLimit (0),
    m_deviceRateLimitFlag (false),
    m_CAWCEnable (false),
    m_feedbackTimer (Timer::CANCEL_ON_DESTROY)
{
}

BwmLocalAgent::~BwmLocalAgent ()
{
  m_timer.Cancel ();
  m_subTimer.Cancel ();
  m_feedbackTimer.Cancel ();
}

void
BwmLocalAgent::SetHostId (uint32_t hostId)
{
  m_hostId = hostId;
}

uint32_t
BwmLocalAgent::GetHostId (void)
{
  return m_hostId;
}

void
BwmLocalAgent::SetQueueDisc (Ptr<BwmQueueDisc> qdisc)
{
  m_qdisc = qdisc;
}

void
BwmLocalAgent::SetCoordinator (Ptr<BwmCoordinator> coordinator)
{
  m_coordinator = coordinator;
}

bool
BwmLocalAgent::CheckIP (Ipv4Address addr)
{
  if (m_ipv4Addr == addr)
    {
      return true;
    }

  return false;
}

void
BwmLocalAgent::SetNewTargetStatus (double newTargetStatus)
{
  m_targetStatus = newTargetStatus;
}

Ptr<UnitFlow>
BwmLocalAgent::AddNewUnitFlow (uint32_t tenantId, uint32_t flowId, uint32_t traceId, Ptr<BwmQueueDiscClass> qDiscClass, Ipv4Address src, Ipv4Address dst)
{
  // try to register the new flow in the coordinator and get the assigned bandwidth function
  std::stringstream ss;
  ss << src.Get () << " " << dst.Get () << " " << m_deviceRateLimit; /* The info str contains src and dst addr, device rate limit*/
  Ptr<UnitFlow> flow = m_coordinator->RegisterFlow (tenantId, flowId, traceId, ss.str ());

  // initialize the data structure of the new unit flow & queue disc class
  if (flow == NULL)
    {
      NS_LOG_WARN ("Invalid flow, cannot register!");
      return NULL;
    }
  m_flowTable.push_back (std::make_pair (flow, qDiscClass));
  /// set the initial rate for the new flow and
  /// expropriate the rate from other local unit flows belonging to the same tenant
  double rateSum = 0;
  std::list<Ptr<BwmQueueDiscClass>> siblingList;
  for (auto it : m_flowTable)
    {
      if (it.first->GetTenantId () == tenantId && it.first->GetFlowId () != flowId)
        {
          siblingList.push_back (it.second);
        }
    }

  if (siblingList.empty())
    {
      //// No sibling, use standard initial rate
      qDiscClass->SetRate (m_deviceRateLimit / 10);
    }
  else
    {
      for (auto it : siblingList)
        {
          rateSum += it->GetRate ().GetBitRate ();
        }
      //// use the average rate as the initial rate
      double initRate = rateSum / (siblingList.size () + 1);
      qDiscClass->SetRate (initRate);
      for (auto it : siblingList)
        {
          //// expropriate rates proportionally
          it->SetRate (it->GetRate ().GetBitRate () - initRate * (it->GetRate ().GetBitRate () / rateSum));
        }
    }

  return flow;
}

uint32_t
BwmLocalAgent::AssignFlowId (uint32_t tenantId, Ipv4Address src, Ipv4Address dst)
{
  // use 32 bit hash function to generate an approximately unique flow id
  std::stringstream ss;
  ss << tenantId << src.Get () << dst.Get ();
  return Hash32 (ss.str ());
}

void
BwmLocalAgent::Update ()
{
  NS_LOG_INFO ("Host " << m_hostId << " updating @ " << Simulator::Now ().GetSeconds ());

  // report the usage information in last cycle to the coordinator
  ReportUsage ();

  // clear usage of all unit flows
  ClearUsage ();

  // schedule next update
  m_timer.Schedule (m_reportCycle);
}

void
BwmLocalAgent::SetupCAWC (Ptr<Ipv4> ipv4)
{
  ipv4->TraceConnectWithoutContext ("Rx", MakeBoundCallback (RxHandler, this));

  // set up timer for CAWC & schedule the first update
  m_feedbackTimer.SetFunction (&BwmLocalAgent::CAWCCheck, this);
  m_feedbackTimer.Schedule (m_feedbackCycle);
  m_CAWCEnable = true;
}

void
BwmLocalAgent::CAWCCheck ()
{
  for (auto entry : m_scoreboard)
    {
      if (Simulator::Now ().GetNanoSeconds () - (int64_t)entry.second[BwmLocalAgent::LMT] > m_feedbackCycle.GetNanoSeconds ())
        {
          //clear outdated data
          entry.second[BwmLocalAgent::SPC] = 0;
          entry.second[BwmLocalAgent::CEB] = 0;
          entry.second[BwmLocalAgent::NMB] = 0;
        }

      if (entry.second[BwmLocalAgent::SPC] > m_feedbackThreshold * 0.2)
        {
          float congestion_factor = entry.second[BwmLocalAgent::CEB] / static_cast<float>(entry.second[BwmLocalAgent::CEB] + entry.second[BwmLocalAgent::NMB]);
          uint8_t* buffer = (uint8_t*)&congestion_factor;
          Ptr<Packet> feedback = Create<Packet, uint8_t const *, uint32_t> (buffer, 4);
          SocketIpTosTag tosTag;
          tosTag.SetTos (0x80);
          feedback->AddPacketTag (tosTag);
          FlowIdTag idTag;
          idTag.SetFlowId (entry.first);
          feedback->AddPacketTag (idTag);
          auto ipv4 = m_node->GetObject<Ipv4> ();
          NS_ASSERT (ipv4 != NULL);
          ipv4->Send (feedback, m_ipv4Addr, Ipv4Address (entry.second[BwmLocalAgent::SRC]), 0xFD, NULL);
          entry.second[BwmLocalAgent::SPC] = 0;
        }
    }

  m_feedbackTimer.Schedule (m_feedbackCycle);
}

void
BwmLocalAgent::RxHandler (Ptr<BwmLocalAgent> agent, Ptr<const Packet> packet, Ptr<Ipv4> ipv4, uint32_t interface)
{
  NS_ASSERT (agent);
  //peek headers
  TcpHeader tcpHeader;
  Ipv4Header ipHeader;
  FlowIdTag idTag;
  Ptr<Packet> p = packet->Copy ();
  bool idValid = false;

  if (packet->PeekHeader (ipHeader) == 0)
    {
      //there should not be a non IP packet in Ipv4L3Protocol
      NS_LOG_WARN ("A packet without IP header");
      NS_ASSERT (0);
    }
  if (!(idValid = packet->PeekPacketTag (idTag)))
    {
      //there could be some packet without flow id tag
      NS_LOG_WARN ("A packet without id tag");
    }

  //check Protocol Number and ToS first
  if (ipHeader.GetProtocol () == 0xFD && ipHeader.GetTos () == 0x80)
    {
      ////if Tos field is set to 1 and Protocol Number is set to 0xFD
      ////then get a congestion feedback from the receiver
      NS_ASSERT (idValid);
      uint32_t flowId = idTag.GetFlowId ();
      p->RemoveHeader (ipHeader);
      float factor = 0.0;
      if (p->CopyData ((uint8_t*)&factor, 4) == 4)
        {
          agent->UpdateCongestionFactor (flowId, factor);
        }

      return;
    }

  p->RemoveHeader (ipHeader);
  if (p->RemoveHeader(tcpHeader) == 0)
    {
      if (idValid)
        {
          //since non-TCP flows have no universal signaling packet,
          //just check every packet that has flow id
          agent->AddSBEntry (idTag.GetFlowId (), ipHeader.GetSource ().Get ());
        }

      if (ipHeader.GetEcn () == Ipv4Header::ECN_CE)
        {
          ////if ECN = 11(CE), then get a ECN from the network, set CE flag
          if (idValid)
            agent->UpdateScoreboard(idTag.GetFlowId (), BwmLocalAgent::CEB, ipHeader.GetPayloadSize (), ipHeader, ipv4);
        }
      else
        {
          ////update normal bytes
          if (idValid)
            agent->UpdateScoreboard (idTag.GetFlowId (), BwmLocalAgent::NMB, ipHeader.GetPayloadSize (), ipHeader, ipv4);
        }
    }
  else
    {
      //check flags
      uint8_t flags = tcpHeader.GetFlags ();
      if (flags & TcpHeader::SYN)
        {
          ///get a SYN, check scoreboard: if no entry for the new flow, try creating one
          if (idValid)
            {
              //only record the flows with flow id
              agent->AddSBEntry (idTag.GetFlowId (), ipHeader.GetSource ().Get ());
            }
        }
      else
        {
          ///got a normal TCP data packet
          if (ipHeader.GetEcn () == Ipv4Header::ECN_CE)
            {
              ////if ECN = 11(CE), then get a ECN from the network, update CE bytes
              if (idValid)
                agent->UpdateScoreboard (idTag.GetFlowId (), BwmLocalAgent::CEB, ipHeader.GetPayloadSize (), ipHeader, ipv4);
            }
          else
            {
              ////update normal bytes
              if (idValid)
                agent->UpdateScoreboard (idTag.GetFlowId (), BwmLocalAgent::NMB, ipHeader.GetPayloadSize (), ipHeader, ipv4);
            }
        }
    }
}

void
BwmLocalAgent::UpdateScoreboard (uint32_t flowId, int flag, uint32_t size, Ipv4Header ipHeader, Ptr<Ipv4> ipv4)
{
  auto it = m_scoreboard.find (flowId);
  NS_ASSERT (it != m_scoreboard.end ());
  NS_ASSERT ((uint32_t)flag < it->second.size () && flag != 0);
  it->second[flag] += size;
  it->second[BwmLocalAgent::SPC]++;

  if (it->second[BwmLocalAgent::SPC] >= m_feedbackThreshold)
    {
      float congestion_factor = it->second[BwmLocalAgent::CEB] / static_cast<float>(it->second[BwmLocalAgent::CEB] + it->second[BwmLocalAgent::NMB]);
      uint8_t* buffer = (uint8_t*)&congestion_factor;
      Ptr<Packet> feedback = Create<Packet, uint8_t const *, uint32_t> (buffer, 4);
      SocketIpTosTag tosTag;
      tosTag.SetTos (0x80);
      feedback->AddPacketTag (tosTag);
      FlowIdTag idTag;
      idTag.SetFlowId (flowId);
      feedback->AddPacketTag (idTag);
      ipv4->Send (feedback, ipHeader.GetDestination (), ipHeader.GetSource (), 0xFD, NULL);
      it->second[BwmLocalAgent::SPC] = 0;
      it->second[BwmLocalAgent::CEB] = 0;
      it->second[BwmLocalAgent::NMB] = 0;
    }

  it->second[BwmLocalAgent::LMT] = Simulator::Now ().GetNanoSeconds ();
}

void
BwmLocalAgent::AddSBEntry (uint32_t flowId, uint32_t srcIp)
{
  if (m_scoreboard.find (flowId) == m_scoreboard.end ())
    {
      std::vector<uint64_t> temp(BwmLocalAgent::SBL_SIZE, 0);
      temp[BwmLocalAgent::SRC] = srcIp;
      m_scoreboard.insert(std::make_pair (flowId, temp));
    }
}

void
BwmLocalAgent::UpdateCongestionFactor (uint32_t flowId, float factor)
{
  for (auto flow : m_flowTable)
    {
      if (flow.first->GetTraceId () == flowId)
        {
          flow.first->SetCongestionFactor (factor);
        }
    }
}

void
BwmLocalAgent::StartApplication ()
{
  // check whether coordinator has been configured & register this host
  NS_ASSERT (m_coordinator);
  m_coordinator->RegisterHost (this);

  // set up timers & schedule the first update
  m_timer.SetFunction (&BwmLocalAgent::Update, this);
  m_timer.Schedule (m_reportCycle);
  m_subTimer.SetFunction (&BwmLocalAgent::TuneRates, this);
  m_subTimer.Schedule (m_tuneCycle);

  // set the device rate limit, only one device by default
  NS_ASSERT (m_qdisc);
  auto device = m_qdisc->GetNetDevice ();
  StringValue rateStr;
  device->GetAttribute ("DataRate", rateStr);
  DataRate deviceRate(rateStr.Get ());
  m_deviceRateLimit = deviceRate.GetBitRate ();

  // get the ipv4 address of the corresponding interface
  auto ipv4 = m_node->GetObject<Ipv4> ();
  auto interface = ipv4->GetInterfaceForDevice (device);
  if (interface == -1)
    {
      NS_LOG_WARN ("The agent hasn't been linked to interface!");
      NS_ASSERT (0);
    }
  m_ipv4Addr = ipv4->GetAddress (interface, 0).GetLocal ();
}

void
BwmLocalAgent::StopApplication ()
{
  // post-process
}

void
BwmLocalAgent::ReportUsage ()
{
  std::list<Ptr<UnitFlow>> flowList;
  for (auto it : m_flowTable)
    {
      // calculate the usage in bandwidth for each unit flow
      it.first->SetBandwidthUsage (it.second->GetUsage () * 1e3 / static_cast<double> (m_reportCycle.GetMilliSeconds ()));
      flowList.push_back (it.first);
    }
  
  m_coordinator->UpdateUsage (this, flowList);
}

void
BwmLocalAgent::ClearUsage ()
{
  for (auto it : m_flowTable)
    {
      // reset the usage for each unit flow
      it.second->ResetUsage ();
    }
}

void
BwmLocalAgent::TuneRates ()
{
  double rateSum = 0;
  // compute the new fair share for each unit flow
  for (auto it : m_flowTable)
    {
      if (!m_CAWCEnable || it.first->GetCongestionFactor () >= m_congestionThreshold || m_deviceRateLimitFlag)
        {
          // without CAWC
          // or in congestion state, follow coordinator
          double oldFS = std::max(it.first->GetAllocatedFS (), 10.0);
          double newFS = oldFS + (m_targetStatus - oldFS) * m_k;
          it.first->SetAllocatedFS (newFS);
          rateSum += it.first->GetAllocatedRate ();
        }
      else if (it.first->GetBandwidthUsage () != 0)
        {
          // not in congestion state and in working state, enforce work-conserving
          double oldFS = std::max(it.first->GetAllocatedFS (), 10.0);
          double newFS = oldFS * (1 + 1.0 / (m_reportCycle / m_tuneCycle));
          it.first->SetAllocatedFS (newFS);
          rateSum += it.first->GetAllocatedRate ();
        }
    }

  if (rateSum == 0)
    {
      rateSum = m_deviceRateLimit * 0.1;
    }

  double scalingFactor = 1.0;
  // check the rate limit of local device
  if (rateSum >= m_deviceRateLimit)
    {
      // enforce the device rate limit
      scalingFactor *= (m_deviceRateLimit / rateSum);
      m_deviceRateLimitFlag = true;
    }
  else
    {
      m_deviceRateLimitFlag = false;
    }

  // set the rate of TBF rate limiter
  double rate = rateSum * scalingFactor;
  if (rate <= 0)
    {
      NS_LOG_INFO ("All rates equal to zero");
      return;
    }

  // set the rate of each bwm qdisc class
  for (auto it : m_flowTable)
    {
      // set the actual rate for each unit flow
      DataRate newRate (it.first->GetAllocatedRate () * scalingFactor);
      it.second->SetRate (newRate);
    }

  m_subTimer.Schedule (m_tuneCycle);
}

}
