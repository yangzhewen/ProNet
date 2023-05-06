#include "ns3/log.h"

#include "flow-weight-tag.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("FlowWeightTag");

NS_OBJECT_ENSURE_REGISTERED (FlowWeightTag);

TypeId
FlowWeightTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::FlowWeightTag")
    .SetParent<Tag> ()
    .SetGroupName("Network")
    .AddConstructor<FlowWeightTag> ()
  ;
  return tid;
}

TypeId
FlowWeightTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

FlowWeightTag::FlowWeightTag ()
  : Tag ()
{
  NS_LOG_FUNCTION (this);
}

FlowWeightTag::FlowWeightTag (double weight)
  : Tag (),
    m_weight (weight)
{
  NS_LOG_FUNCTION (this << weight);
}

void
FlowWeightTag::SetWeight (double weight)
{
  NS_LOG_FUNCTION (this << weight);
  m_weight = weight;
}

double
FlowWeightTag::GetWeight (void) const
{
  NS_LOG_FUNCTION (this);
  return m_weight;
}

void
FlowWeightTag::Serialize (TagBuffer i) const
{
  NS_LOG_FUNCTION (this << &i);
  i.WriteDouble (m_weight);
}

void
FlowWeightTag::Deserialize (TagBuffer i)
{
  NS_LOG_FUNCTION (this << &i);
  m_weight = i.ReadDouble ();
}

uint32_t
FlowWeightTag::GetSerializedSize (void) const
{
  NS_LOG_FUNCTION (this);
  return sizeof (double);
}

void
FlowWeightTag::Print (std::ostream &os) const
{
  NS_LOG_FUNCTION (this << &os);
  os << "FlowWeight=" << m_weight;
}

}
