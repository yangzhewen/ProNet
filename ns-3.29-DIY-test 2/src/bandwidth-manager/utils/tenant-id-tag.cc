#include "tenant-id-tag.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TenantIdTag");

NS_OBJECT_ENSURE_REGISTERED (TenantIdTag);

TypeId 
TenantIdTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TenantIdTag")
    .SetParent<Tag> ()
    .SetGroupName("Network")
    .AddConstructor<TenantIdTag> ()
  ;
  return tid;
}
TypeId 
TenantIdTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}
uint32_t 
TenantIdTag::GetSerializedSize (void) const
{
  NS_LOG_FUNCTION (this);
  return 4;
}
void 
TenantIdTag::Serialize (TagBuffer buf) const
{
  NS_LOG_FUNCTION (this << &buf);
  buf.WriteU32 (m_tenantId);
}
void 
TenantIdTag::Deserialize (TagBuffer buf)
{
  NS_LOG_FUNCTION (this << &buf);
  m_tenantId = buf.ReadU32 ();
}
void 
TenantIdTag::Print (std::ostream &os) const
{
  NS_LOG_FUNCTION (this << &os);
  os << "TenantId=" << m_tenantId;
}
TenantIdTag::TenantIdTag ()
  : Tag () 
{
  NS_LOG_FUNCTION (this);
}

TenantIdTag::TenantIdTag (uint32_t id)
  : Tag (),
    m_tenantId (id)
{
  NS_LOG_FUNCTION (this << id);
}

void
TenantIdTag::SetTenantId (uint32_t id)
{
  NS_LOG_FUNCTION (this << id);
  m_tenantId = id;
}
uint32_t
TenantIdTag::GetTenantId (void) const
{
  NS_LOG_FUNCTION (this);
  return m_tenantId;
}

uint32_t 
TenantIdTag::AllocateTenantId (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  static uint32_t nextTenantId = 1;
  uint32_t tenantId = nextTenantId;
  nextTenantId++;
  return tenantId;
}

} // namespace ns3

