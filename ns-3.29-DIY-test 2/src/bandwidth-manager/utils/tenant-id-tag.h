#ifndef TENANT_ID_TAG_H
#define TENANT_ID_TAG_H

#include "ns3/tag.h"

namespace ns3 {

class TenantIdTag : public Tag
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (TagBuffer buf) const;
  virtual void Deserialize (TagBuffer buf);
  virtual void Print (std::ostream &os) const;
  TenantIdTag ();
  
  /**
   *  Constructs a TenantIdTag with the given tenant id
   *
   *  \param tenantId Id to use for the tag
   */
  TenantIdTag (uint32_t tenantId);
  /**
   *  Sets the tenant id for the tag
   *  \param tenantId Id to assign to the tag
   */
  void SetTenantId (uint32_t tenantId);
  /**
   *  Gets the tenant id for the tag
   *  \returns current tenant id for this tag
   */
  uint32_t GetTenantId (void) const;
  /**
   *  Uses a static variable to generate sequential tenant id
   *  \returns tenant id allocated
   */
  static uint32_t AllocateTenantId (void);
private:
  uint32_t m_tenantId; //!< tenant ID
};

} // namespace ns3

#endif /* TENANT_ID_TAG_H */
