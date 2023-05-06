#include "ns3/log.h"
#include "ns3/string.h"
#include "ns3/tenant-id-tag.h"
#include "ns3/flow-id-tag.h"
#include "ns3/ipv4-header.h"
#include "ns3/tcp-header.h"
#include "ns3/internet-module.h"
//#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

#include "bwm-queue-disc.h"
#include "bwm-local-agent.h"
#include "bwm-coordinator.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BwmQueueDisc");

NS_OBJECT_ENSURE_REGISTERED (BwmQueueDiscClass);

TypeId
BwmQueueDiscClass::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::BwmQueueDiscClass")
    .SetParent<QueueDiscClass> ()
    .SetGroupName ("BandwidthManager")
    .AddConstructor<BwmQueueDiscClass> ()          
    .AddTraceSource ("Rate",
                     "Dynamically configured rate of the unit flow",
                     MakeTraceSourceAccessor (&BwmQueueDiscClass::m_rate),
                     "ns3::TracedValueCallback::DataRate")
    .AddTraceSource ("Usage",
                     "Statistical usage data in bytes",
                     MakeTraceSourceAccessor (&BwmQueueDiscClass::m_usage),
                     "ns3::TracedValueCallback::Double")
  ;
  return tid;
}

BwmQueueDiscClass::BwmQueueDiscClass ()
{
  m_rate = DataRate ("0KB/s");
  m_usage = 0;
  m_traceId = -1;
}

BwmQueueDiscClass::~BwmQueueDiscClass ()
{

}

void
BwmQueueDiscClass::AddUsage (uint32_t pktSize)
{
  m_usage += pktSize;
}

Ptr<QueueDiscItem>
BwmQueueDiscClass::Dequeue ()
{
  NS_LOG_INFO (this);

  return GetQueueDisc ()->Dequeue ();
}

bool
BwmQueueDiscClass::Enqueue (Ptr<QueueDiscItem> item)
{
  NS_LOG_INFO (this << item);

  return GetQueueDisc ()->Enqueue (item);
}

Ptr<QueueDiscItem>
BwmQueueDiscClass::Drop (void)
{
  NS_LOG_INFO (this);

  return GetQueueDisc ()->Dequeue ();
}

bool
BwmQueueDiscClass::SetRate (DataRate rate)
{
  // illegal rate arguments
  if (rate <= DataRate ("0KB/s"))
    {
      NS_LOG_INFO ("Meet illegal rate args " << rate);
      return false;
    }

  // record the new rate parameter
  m_rate = rate;

  // set the rate of internal TBF qdisc
  Ptr<TbfQueueDisc> rateLimiter = DynamicCast<TbfQueueDisc, QueueDisc> (GetQueueDisc ());
  DataRate newRate (rate);
  rateLimiter->SetRate (newRate);

  // set the peak rate of internal TBF qdisc
  DataRate newPeakRate (rate.GetBitRate () << 1);
  rateLimiter->SetPeakRate (newPeakRate);

//  NS_LOG_INFO ("Set the rate of flow " << m_traceId << " to " << rate << m_rateUnit);

  return true;
}

DataRate
BwmQueueDiscClass::GetRate () const
{
  return m_rate;
}

void
BwmQueueDiscClass::SetFlowId (uint32_t flowId)
{
  m_flowId = flowId;
}

uint32_t
BwmQueueDiscClass::GetFlowId () const
{
  return m_flowId;
}

uint32_t
BwmQueueDiscClass::GetTraceId () const
{
  return m_traceId;
}

void
BwmQueueDiscClass::SetTraceId (uint32_t traceId)
{
  m_traceId = traceId;
}

double
BwmQueueDiscClass::GetUsage () const
{
  return m_usage * 8;
}

void
BwmQueueDiscClass::ResetUsage ()
{
  m_usage = 0;
}

NS_OBJECT_ENSURE_REGISTERED (BwmQueueDisc);

TypeId
BwmQueueDisc::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::BwmQueueDisc")
    .SetParent<QueueDisc> ()
    .SetGroupName ("BandwidthManager")
    .AddConstructor<BwmQueueDisc> ()
    .AddAttribute ("MaxSize",
                   "The maximum number of packets accepted by this queue disc",
                   QueueSizeValue (QueueSize ("10240p")),
                   MakeQueueSizeAccessor (&QueueDisc::SetMaxSize,
                                          &QueueDisc::GetMaxSize),
                   MakeQueueSizeChecker ())
    .AddAttribute ("Flows",
                   "The number of queues into which the incoming packets are classified",
                   UintegerValue (1031),
                   MakeUintegerAccessor (&BwmQueueDisc::SetFlowNum),
                   MakeUintegerChecker<uint32_t> ())
    .AddTraceSource ("FlowCreate",
                     "Create an internal queue disc class for a unit-flow",
                     MakeTraceSourceAccessor (&BwmQueueDisc::m_flowCreateTrace),
                     "ns3::BwmQueueDisc::QueueDiscClassTracedCallback")               
  ;
  return tid;
}

BwmQueueDisc::BwmQueueDisc ()
{
  m_nextFlow = 0;
}

BwmQueueDisc::~BwmQueueDisc ()
{
  
}

void
BwmQueueDisc::SetFlowNum (uint32_t flowNum)
{
  m_flowNum = flowNum;
}

void
BwmQueueDisc::SetupLocalAgent (Ptr<BwmLocalAgent> agent)
{
  m_agent = agent;
}

bool
BwmQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
{
  NS_LOG_FUNCTION (this << item);

  if (m_agent == NULL)
    {
      NS_LOG_WARN ("The local agent hasn't been registered!");
      NS_ASSERT (0);
    }

  // check queue size and drop packets
  if (GetCurrentSize () > GetMaxSize ())
    {
      DropBeforeEnqueue (item, "Overlimit drop");
      return false;
    }

  uint32_t index = 0;

  // extract tenant id from the packet
  TenantIdTag tidTag;
  uint32_t tenantId;
  uint32_t flowId;
  Ipv4Header ipv4H;
  if (!item->GetPacket ()->PeekPacketTag (tidTag))
    {
      NS_LOG_LOGIC ("The item has no tenant id tag!");
      NS_LOG_LOGIC ("Assign it to the default queue disc class!");

      tenantId = -1;
      flowId = -1;
    }
  else
    {
      // extract the tenant id from the packet
      tenantId = tidTag.GetTenantId ();

      // request the flow id from the local agent
//      item->GetPacket ()->PeekHeader (ipv4H);
      Ipv4QueueDiscItem* iqdt = dynamic_cast<Ipv4QueueDiscItem*> (GetPointer(item));
      NS_ASSERT (iqdt);
      ipv4H = iqdt->GetHeader();
      flowId = m_agent->AssignFlowId (tenantId, ipv4H.GetSource (), ipv4H.GetDestination ());
    }

  // assign a queue disc class to the flow
  index = flowId % m_flowNum;

  // extract trace id
  FlowIdTag fidTag;
  item->GetPacket ()->PeekPacketTag (fidTag);
  uint32_t traceId = fidTag.GetFlowId ();

  Ptr<BwmQueueDiscClass> flow = NULL;
  if (tenantId == (unsigned)-1 && flowId == (unsigned)-1)
    {
      // unclassified item, guide it into the default queue disc class
      NS_LOG_INFO ("Meet an unclassified item " << item);
      flow = StaticCast<BwmQueueDiscClass> (GetQueueDiscClass (0));
    }

  while (flow == NULL)
    {
      if (m_flowNumIndices.find (index) == m_flowNumIndices.end ())
      {
        // create a new queue disc class for the new flow
        NS_LOG_DEBUG ("Creating a new flow queue with index " << index);
        flow = m_queueDiscClassFactory.Create<BwmQueueDiscClass> ();
        Ptr<TbfQueueDisc> qd = m_queueDiscFactory.Create<TbfQueueDisc> ();
        qd->SetNetDevice(GetNetDevice());
        qd->Initialize ();
        qd->SetBwmQdiscClass (flow);
        flow->SetQueueDisc (qd);
        AddQueueDiscClass (flow);
        m_flowNumIndices[index] = GetNQueueDiscClasses () - 1;
        flow->SetTraceId (traceId);
        flow->SetFlowId (flowId);

        m_flowCreateTrace (flow);

        // record this new flow in Bwm Data structure
        auto flowRecord = m_agent->AddNewUnitFlow (tenantId, flowId, traceId, flow, ipv4H.GetSource (), ipv4H.GetDestination ());
        if (!flowRecord)
          {
            NS_LOG_WARN ("Cannot create new unit flow for the item: " << item);
            NS_ASSERT (0);
          }
      }
    else
      {
        // find the corresponding queue disc class
        flow = StaticCast<BwmQueueDiscClass> (GetQueueDiscClass (m_flowNumIndices[index]));
        if (flow->GetFlowId () != flowId)// || flow->GetTraceId() != traceId)
          {
            // meet collision, use linear probe to handle
            NS_LOG_INFO ("A hash collision emerges");
            index = (index + 1) % m_flowNum;
            flow = NULL;
          }
      }
    }

  bool retval = flow->Enqueue (item);
  return retval;
}

Ptr<QueueDiscItem>
BwmQueueDisc::DoDequeue (void)
{
  NS_LOG_FUNCTION (this);

  if (m_flowNumIndices.size () == 0)
    {
      NS_LOG_LOGIC ("Queue empty");
      return NULL;
    }

  auto end = m_nextFlow;
  uint32_t upperBound = GetNQueueDiscClasses ();
  Ptr<BwmQueueDiscClass> flow;
  Ptr<QueueDiscItem> item = NULL;
  do
    {
      flow = StaticCast<BwmQueueDiscClass> (GetQueueDiscClass (m_nextFlow));
      item = flow->Dequeue ();
      if (item) 
        {
          NS_LOG_LOGIC ("Dequeue a valid item normally");
          m_nextFlow = (m_nextFlow + 1) % upperBound;
          break;
        }
      else
        {
          NS_LOG_LOGIC ("No item for dequeuing");
          m_nextFlow = (m_nextFlow + 1) % upperBound;
        }
    } while (end != m_nextFlow);

  return item;
}

void
BwmQueueDisc::InitializeParams ()
{
  // set BwmQueueDiscClass as the internal queue disc class
  m_queueDiscClassFactory.SetTypeId ("ns3::BwmQueueDiscClass");

  // set TbfQueueDisc as the internal queue disc
  m_queueDiscFactory.SetTypeId ("ns3::TbfQueueDisc");
  m_queueDiscFactory.Set ("MaxSize", QueueSizeValue (GetMaxSize ()));

  // create the default unlimited queue disc class
  auto flow = m_queueDiscClassFactory.Create<BwmQueueDiscClass> ();
  auto device = GetNetDevice ();
  Ptr<TbfQueueDisc> qd = m_queueDiscFactory.Create<TbfQueueDisc> ();
  qd->SetNetDevice(device);
  qd->Initialize ();
  flow->SetQueueDisc (qd);
  AddQueueDiscClass (flow);

  // configure the default unlimited queue disc class
  StringValue rateStr;
  device->GetAttribute ("DataRate", rateStr);
  DataRate deviceRate(rateStr.Get ());
  DataRate defaultQueueRate(deviceRate.GetBitRate () >> 1);
  flow->SetRate (defaultQueueRate);
  // construct a mapping from -1 to the default queue disc class
  m_flowNumIndices[-1] = 0;
}

bool
BwmQueueDisc::CheckConfig (void)
{
  NS_LOG_INFO (this);

  if (GetNQueueDiscClasses () > 0)
    {
      NS_LOG_ERROR ("BwmQueueDisc cannot have classes");
      return false;
    }

  if (GetNInternalQueues () > 0)
    {
      NS_LOG_ERROR ("BwmQueueDisc cannot have internal queues");
      return false;
    }

  return true;
}

void
BwmQueueDisc::Drop (void)
{
  NS_LOG_FUNCTION (this);

  while (GetCurrentSize () > GetMaxSize ())
    {
      Ptr<BwmQueueDiscClass> flow;
      // random select a subqueue
      NS_ASSERT (m_flowNumIndices.size () != 0);
      int randNum = rand () % m_flowNumIndices.size ();
      flow = StaticCast<BwmQueueDiscClass> (GetQueueDiscClass (randNum));
      NS_ASSERT (flow);

      // drop the item from the flow
      Ptr<QueueDiscItem> item = flow->Drop ();
      if (item)
        DropAfterDequeue (item, "Overlimit drop");
    }
}

}
