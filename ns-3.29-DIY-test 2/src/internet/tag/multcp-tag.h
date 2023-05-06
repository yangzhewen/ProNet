#ifndef MULTCP_TAG_H
#define MULTCP_TAG_H

#include "ns3/tag.h"

namespace ns3 {

class MultcpTag : public Tag
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;

  /**
   * Create a MultcpTag with default value
   */
  MultcpTag ();

  // inherited from Tag
  virtual void Serialize (TagBuffer i) const;
  virtual void Deserialize (TagBuffer i);
  virtual uint32_t GetSerializedSize () const;
  virtual void Print (std::ostream &os) const;

  // getter and setter for MultcpTag
  uint32_t GetFlowId () const;
  
  void SetFlowId (uint32_t fid);

private:
  uint32_t m_flowId;	// <- tenant id
};

} // namespace ns3

#endif // MULTCP_TAG_H
