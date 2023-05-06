/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include <limits>

#include "ns3/flow-weight-tag.h"
#include "ns3/log.h"
#include "ns3/string.h"

#include "wfq-queue-disc.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("WfqQueueDisc");

NS_OBJECT_ENSURE_REGISTERED (WfqFlow);

TypeId WfqFlow::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::WfqFlow")
    .SetParent<QueueDiscClass> ()
    .SetGroupName ("TrafficControl")
    .AddConstructor<WfqFlow> ()
    .AddAttribute ("DefaultWeight",
                   "Default weight used for packets without FlowWeightTag.",
                   DoubleValue (1.0),
                   MakeDoubleAccessor (&WfqFlow::m_defaultWeight),
                   MakeDoubleChecker<double> (0.0))
    .AddAttribute ("Weight",
                   "Static configured weight.",
                   DoubleValue (0.0),
                   MakeDoubleAccessor (&WfqFlow::m_weight),
                   MakeDoubleChecker<double> (0.0))
  ;
  return tid;
}

WfqFlow::WfqFlow ()
  : m_status (INACTIVE),
    m_headTs (0.0),
    m_tailTs (0.0)
{
  NS_LOG_FUNCTION (this);
}

WfqFlow::~WfqFlow ()
{
  NS_LOG_FUNCTION (this);
}

WfqFlow::FlowStatus
WfqFlow::GetStatus (void) const
{
  NS_LOG_FUNCTION (this);
  return m_status;
}

bool
WfqFlow::Enqueue (Ptr<QueueDiscItem> item, double ts)
{
  NS_LOG_FUNCTION (this << item << ts);

  double weight = GetWeight (item);

  if (m_status == INACTIVE)
    {
      m_status = ACTIVE;
      // update head_ts iff the flow status changes
      m_headTs = ts + item->GetSize () / weight;
      m_tailTs = ts;
    }
  m_tailTs += item->GetSize () / weight;

  return GetQueueDisc ()->Enqueue (item);
}

Ptr<QueueDiscItem>
WfqFlow::Dequeue (void)
{
  NS_LOG_FUNCTION (this);

  if (m_status == INACTIVE)
    {
      // the queue disc is empty
      return 0;
    }

  Ptr<QueueDiscItem> item = GetQueueDisc ()->Dequeue ();

  if (GetQueueDisc ()->GetNPackets ())
    {
      // the queue disc is not empty: update head_ts
      Ptr<const QueueDiscItem> head = GetQueueDisc ()->Peek ();
      if (!head)
        {
          NS_LOG_INFO ("Cannot peek a packet, pass updating!");
          return item;
        }
      double weight = GetWeight (head);
      m_headTs += head->GetSize () / weight;
    }
  else
    {
      m_status = INACTIVE;
    }

  return item;
}

Ptr<QueueDiscItem>
WfqFlow::Drop (void)
{
  NS_LOG_FUNCTION (this);

  if (m_status == INACTIVE)
    {
      // the queue disc is empty
      return 0;
    }

  Ptr<QueueDiscItem> item = GetQueueDisc ()->Dequeue ();

  // update ts and flow status after dequeue
  if (GetQueueDisc ()->GetNPackets ())
    {
      // the queue disc is not empty: recalculate head_ts and tail_ts
      double diff_time = item->GetSize () / GetWeight (item);
      Ptr<const QueueDiscItem> head = GetQueueDisc ()->Peek ();
      m_headTs = m_headTs - diff_time + head->GetSize () / GetWeight (head);
      m_tailTs -= diff_time;
    }
  else
    {
      m_status = INACTIVE;
    }
  return item;
}

double
WfqFlow::GetHeadTs (void) const
{
  NS_LOG_FUNCTION (this);
  return m_headTs;
}

double
WfqFlow::GetTailTs (void) const
{
  NS_LOG_FUNCTION (this);
  return m_tailTs;
}

double
WfqFlow::GetWeight (Ptr<const QueueDiscItem> item) const
{
  NS_LOG_FUNCTION (this << item);

  // the flow has a configured weight: use the configured weight
  if (m_weight != 0)
    {
      return m_weight;
    }

  // the flow has not designated weight: extract weight from packets dynamically
  double weight = m_defaultWeight;
  FlowWeightTag weightTag;
  if (item->GetPacket ()->PeekPacketTag (weightTag))
    {
      weight = weightTag.GetWeight ();
    }

  return weight;
}

void
WfqFlow::SetWeight (double weight)
{
  m_weight = weight;
}

NS_OBJECT_ENSURE_REGISTERED (WfqQueueDisc);

TypeId WfqQueueDisc::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::WfqQueueDisc")
    .SetParent<QueueDisc> ()
    .SetGroupName ("TrafficControl")
    .AddConstructor<WfqQueueDisc> ()
    .AddAttribute ("MaxSize",
                   "The maximum number of packets accepted by this queue disc",
                   QueueSizeValue (QueueSize ("10240p")),
                   MakeQueueSizeAccessor (&QueueDisc::SetMaxSize,
                                          &QueueDisc::GetMaxSize),
                   MakeQueueSizeChecker ())
    .AddAttribute ("Flows",
                   "The number of queues into which the incoming packets are classified",
                   UintegerValue (1031),
                   MakeUintegerAccessor (&WfqQueueDisc::m_flows),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Perturbation",
                   "The salt used as an additional input to the hash function used to classify packets",
                   UintegerValue (0),
                   MakeUintegerAccessor (&WfqQueueDisc::m_perturbation),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("InternalQueueDiscClassTypeId",
                   "The TypeId of the internal queue disc class",
                   StringValue ("ns3::WfqFlow"),
                   MakeStringAccessor (&WfqQueueDisc::m_queueDiscClassTypeId),
                   MakeStringChecker ())
    .AddAttribute ("InternalQueueDiscTypeId",
                   "The TypeId of the internal queue disc",
                   StringValue ("ns3::FifoQueueDisc"),
                   MakeStringAccessor (&WfqQueueDisc::m_queueDiscTypeId),
                   MakeStringChecker ())
  ;
  return tid;
}

WfqQueueDisc::WfqQueueDisc ()
  : QueueDisc (QueueDiscSizePolicy::MULTIPLE_QUEUES),
    m_currentTs (0.0)
{
  NS_LOG_FUNCTION (this);
}

WfqQueueDisc::~WfqQueueDisc ()
{
  NS_LOG_FUNCTION (this);
}

void
WfqQueueDisc::SetNFlows (uint32_t flowNum)
{
  m_flows = flowNum;
}

bool
WfqQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
{
  NS_LOG_FUNCTION (this << item);
  uint32_t h = 0;

  if (GetNPacketFilters () == 0)
    {
      h = item->Hash (m_perturbation) % m_flows;
    }
  else
    {
      int32_t ret = Classify (item);

      if (ret != PacketFilter::PF_NO_MATCH)
        {
          h = ret % m_flows;
        }
      else
        {
          NS_LOG_ERROR ("No filter has been able to classify this packet, drop it.");
          DropBeforeEnqueue (item, UNCLASSIFIED_DROP);
          return false;
        }
    }

  Ptr<WfqFlow> flow;
  if (m_flowsIndices.find (h) == m_flowsIndices.end ())
    {
      NS_LOG_DEBUG ("Creating a new flow queue with index " << h);
      flow = m_queueDiscClassFactory.Create<WfqFlow> ();
      Ptr<QueueDisc> qd = m_queueDiscFactory.Create<QueueDisc> ();
      qd->Initialize ();
      flow->SetQueueDisc (qd);
      AddQueueDiscClass (flow);

      m_flowsIndices[h] = GetNQueueDiscClasses () - 1;
    }
  else
    {
      flow = StaticCast<WfqFlow> (GetQueueDiscClass (m_flowsIndices[h]));
    }

  bool retval = flow->Enqueue (item, m_currentTs);

  if (flow->GetStatus () == WfqFlow::ACTIVE)
    {
      m_activeFlows.insert (flow);
    }

  // check queue size and drop packets
  if (GetCurrentSize () > GetMaxSize ())
    {
      WfqDrop ();
    }

  return retval;
}

Ptr<QueueDiscItem>
WfqQueueDisc::DoDequeue (void)
{
	NS_LOG_FUNCTION (this);

	Ptr<WfqFlow> flow;
	double minTs = std::numeric_limits<double>::max ();

	// find the active flow with lowest head_ts
	for (Ptr<WfqFlow> i : m_activeFlows)
	  {
	    if (i->GetHeadTs () < minTs)
	      {
	        flow = i;
	        minTs = i->GetHeadTs ();
	      }
	  }

  if (!flow)
	  {
	    NS_LOG_LOGIC ("Queue empty");
	    return 0;
	  }

	Ptr<QueueDiscItem> item = flow->Dequeue ();
	NS_ASSERT (item);

	// update active flow set
	if (flow->GetStatus () != WfqFlow::ACTIVE)
	  {
	    m_activeFlows.erase (flow);
	  }

	// note that currentTs may be greater than minTs
	m_currentTs = std::max (minTs, m_currentTs);
	return item;

}

Ptr<const QueueDiscItem>
WfqQueueDisc::DoPeek (void)
{
  NS_LOG_FUNCTION (this);

  Ptr<WfqFlow> flow;
  double minTs = std::numeric_limits<double>::max ();

  // find the active flow with lowest head_ts
  for (Ptr<WfqFlow> i : m_activeFlows)
    {
      if (i->GetHeadTs () < minTs)
        {
          flow = i;
          minTs = i->GetHeadTs ();
        }
    }

  if (!flow)
    {
      NS_LOG_LOGIC ("Queue empty");
      return 0;
    }

  // do not update timestamp when calling peek()
  return flow->GetQueueDisc ()->Peek ();
}

bool
WfqQueueDisc::CheckConfig (void)
{
  NS_LOG_FUNCTION (this);

  if (GetNQueueDiscClasses () > 0)
    {
      NS_LOG_ERROR ("WfqQueueDisc cannot have classes");
      return false;
    }

  if (GetNInternalQueues () > 0)
    {
      NS_LOG_ERROR ("WfqQueueDisc cannot have internal queues");
      return false;
    }

  return true;
}

void
WfqQueueDisc::InitializeParams (void)
{
  NS_LOG_FUNCTION (this);

  // set WfqFlow as the internal queue disc class
  m_queueDiscClassFactory.SetTypeId (m_queueDiscClassTypeId);

  // set Fifo as the internal queue disc for WfqFlow
  m_queueDiscFactory.SetTypeId (m_queueDiscTypeId);
  m_queueDiscFactory.Set ("MaxSize", QueueSizeValue (GetMaxSize ()));
}

void
WfqQueueDisc::WfqDrop (void)
{
  NS_LOG_FUNCTION (this);

  while (GetCurrentSize () > GetMaxSize ())
    {
      Ptr<WfqFlow> flow;
      double maxTs = std::numeric_limits<double>::min ();
      // find the active flow with highest tail_ts
      for (Ptr<WfqFlow> i : m_activeFlows)
        {
          if (i->GetTailTs () > maxTs)
            {
              flow = i;
              maxTs = i->GetTailTs ();
            }
        }
      NS_ASSERT (flow);

      Ptr<QueueDiscItem> item = flow->Drop ();
      NS_ASSERT (item);

      DropAfterDequeue (item, OVERLIMIT_DROP);

      // update active flow set
      if (flow->GetStatus () != WfqFlow::ACTIVE)
        {
          m_activeFlows.erase (flow);
        }
    }
}

}	// namespace ns3
