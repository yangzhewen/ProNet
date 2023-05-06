#ifndef WFQ_QUEUE_DISC_H
#define WFQ_QUEUE_DISC_H

#include "ns3/object-factory.h"
#include "ns3/queue-disc.h"
#include "ns3/tag.h"

namespace ns3 {

class WfqFlow : public QueueDiscClass {
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  /**
   * \brief WfqFlow constructor
   */
  WfqFlow ();

  virtual ~WfqFlow ();

  /**
   * \enum FlowStatus
   * \brief Used to determine the status of this flow queue
   */
  enum FlowStatus
    {
      INACTIVE,
      ACTIVE,
    };

  /**
   * \brief Get the status of this flow
   * \return the status of this flow
   */
  FlowStatus GetStatus (void) const;
  /**
   * Pass a packet to store to the flow.
   * \param item item to enqueue
   * \param ts current virtual timestamp
   * \return True if the operation was successful; false otherwise
   */
  virtual bool Enqueue (Ptr<QueueDiscItem> item, double ts);
  /**
   * Dequeue a packet from the flow.
   * \return 0 if the operation was not successful; the item otherwise.
   */
  virtual Ptr<QueueDiscItem> Dequeue (void);
  /**
   * Drop a packet from the flow.
   * \return 0 if the operation was not successful; the item otherwise.
   */
  virtual Ptr<QueueDiscItem> Drop (void);
  /**
   * Get timestamp of the first packet.
   * \return timestamp
   */
  double GetHeadTs (void) const;
  /**
   * Get timestamp of the last packet.
   * \return timestamp
   */
  double GetTailTs (void) const;
  /**
   * Get the weight of this flow.
   * \return flow weight.
   */
  double GetWeight (Ptr<const QueueDiscItem> item) const;
  /**
   * Set the weight of this flow.
   */
  void SetWeight (double weight);

private:
  double m_defaultWeight;	//!< default weight used for packets without FlowWeightTag
  double m_weight; //!< weight set by administrator
  FlowStatus m_status;	//!< the status of this flow
  double m_headTs;	//!< finish timestamp of the first pkt
  double m_tailTs;	//!< finish timestamp of the last pkt
};

class WfqQueueDisc : public QueueDisc {
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  /**
   * \brief WfqQueueDisc constructor
   */
  WfqQueueDisc ();

  virtual ~WfqQueueDisc ();

  /**
   * \brief Set the number of flow queues
   */
  void SetNFlows (uint32_t flowNum);

  // Reasons for dropping packets
  static constexpr const char* UNCLASSIFIED_DROP = "Unclassified drop";  //!< No packet filter able to classify packet
  static constexpr const char* OVERLIMIT_DROP = "Overlimit drop";        //!< Overlimit dropped packets

private:
  virtual bool DoEnqueue (Ptr<QueueDiscItem> item);
  virtual Ptr<QueueDiscItem> DoDequeue (void);
  virtual Ptr<const QueueDiscItem> DoPeek (void);
  virtual bool CheckConfig (void);
  virtual void InitializeParams (void);

protected:
  /**
   * \brief Drop packets until CurrentSize < MaxSize.
   */
  void WfqDrop (void);

  double m_currentTs;	//!< current timestamp

  uint32_t m_perturbation;	//!< hash perturbation value
  uint32_t m_flows;	//!< Number of flow queues

  std::map<uint32_t, uint32_t> m_flowsIndices;	//!< Map with the index of class for each flow
  std::set<Ptr<WfqFlow> > m_activeFlows;	//!< Set of active flows

  ObjectFactory m_queueDiscClassFactory; //!< Factory to create a new internal queue disc class
  ObjectFactory m_queueDiscFactory;	//!< Factory to create a new internal child queue disc
  std::string m_queueDiscClassTypeId; //!< TypeId of the internal queue disc classs
  std::string m_queueDiscTypeId; //!< TypeId of the internal queue disc
};

}	// namespace ns3

#endif // WFQ_QUEUE_DISC_H
