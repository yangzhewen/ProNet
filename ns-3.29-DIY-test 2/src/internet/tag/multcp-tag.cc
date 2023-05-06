#include "multcp-tag.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (MultcpTag);

TypeId
MultcpTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MultcpTag")
      .SetParent<Tag> ()
      .SetGroupName ("Internet")
      .AddConstructor<MultcpTag> ()
  ;
  return tid;
}

TypeId
MultcpTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

MultcpTag::MultcpTag ()
  : m_flowId(0)
{
}

void
MultcpTag::Serialize (TagBuffer i) const
{
  i.WriteU32 (m_flowId);
}

void
MultcpTag::Deserialize (TagBuffer i)
{
  m_flowId = i.ReadU32 ();
}

uint32_t
MultcpTag::GetSerializedSize () const
{
  return sizeof (m_flowId);
}

void
MultcpTag::Print (std::ostream &os) const
{
  os << "Multcp Flow Id: " << m_flowId;
}

uint32_t
MultcpTag::GetFlowId () const
{
  return m_flowId;
}

void
MultcpTag::SetFlowId (uint32_t fid)
{
  m_flowId = fid;
}

} // namespace ns3
