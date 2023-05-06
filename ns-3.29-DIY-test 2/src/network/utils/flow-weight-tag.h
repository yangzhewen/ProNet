#ifndef FLOW_WEIGHT_TAG_H
#define FLOW_WEIGHT_TAG_H

#include "ns3/tag.h"

namespace ns3 {

/**
 * \brief the weight of the flow
 */
class FlowWeightTag : public Tag
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;

  FlowWeightTag ();
  /**
   *  Constructs a FlowWeightTag with the given flow weight
   *
   *  \param weight weight to use for the tag
   */
  FlowWeightTag (double weight);
  /**
   * \brief Set the tag's weight
   *
   * \param weight the weight
   */
  void SetWeight (double weight);
  /**
   * \brief Get the tag's weight
   *
   * \returns the weight
   */
  double GetWeight (void) const;

  // inherited function, no need to doc.
  virtual void Serialize (TagBuffer i) const;
  virtual void Deserialize (TagBuffer i);
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Print (std::ostream &os) const;

private:
  double m_weight;	//!< the weight carried by the tag
};

}	// namespace ns3

#endif // FLOW_WEIGHT_TAG_H
