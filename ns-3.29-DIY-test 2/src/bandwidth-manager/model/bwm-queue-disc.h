#ifndef BWM_QUEUE_DISC_H
#define BWM_QUEUE_DISC_H

#include "ns3/object-factory.h"
#include "ns3/queue-disc.h"
#include "ns3/tbf-queue-disc.h"
#include "ns3/wfq-queue-disc.h"

namespace ns3 {

class BwmLocalAgent;

/**
 * \ingroup bandwidth-manager
 *
 * \brief A queue discipline class for a unit flow
 * 
 * This queue disc class uses TBF qdisc as the internal queue disc.
 */
class BwmQueueDiscClass : public QueueDiscClass {
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  /**
   *  \brief BwmQueueDiscClass constructor
   */
  BwmQueueDiscClass ();

  ~BwmQueueDiscClass ();

  /**
   * \brief Dequeue a packet from the flow and record its size.
   * \return 0 if the operation was not successful; the item otherwise.
   * 
   * Override the original "Dequeue" function, add statistics about usage info
   */
  virtual Ptr<QueueDiscItem> Dequeue (void);
  /**
   * Pass a packet to the flow.
   * \param item item to enqueue
   * \return True if the operation was successful; false otherwise
   */
  virtual bool Enqueue (Ptr<QueueDiscItem> item);
  /**
   * Drop a packet from the flow.
   * \return The dropped item or NULL
   */
  Ptr<QueueDiscItem> Drop (void);
  /**
   *  \brief Set rate of the flow without considering the rate unit.
   *  \return true if the operation succeeds, false otherwise.
   */
  bool SetRate (DataRate rate);
  /**
   *  \brief Get the rate of the flow without considering the rate unit.
   *  \return the rate in bps.
   */
  DataRate GetRate (void) const;
  /**
   *  \brief Set the size in bytes of the first token bucket of the internal TBF queue disc.
   *  \return true if the operation succeeds, false otherwise.
   */
  bool SetBurst (uint32_t burst);
  /**
   *  \brief Set the flow id.
   */
  void SetFlowId (uint32_t flowId);
  /**
   *  \brief Get the id of this flow.
   *  \return flow id.
   */
  uint32_t GetFlowId (void) const;
  /**
   *  \brief Get the usage of this flow in the last sampling interval.
   *  \return usage in bits.
   */
  double GetUsage (void) const;
  /**
   *  \brief Reset the usage to 0.
   */
  void ResetUsage (void);
  /**
   * \brief Get the unique trace id of corresponding unit flow.
   * \return m_traceId.
   */
  uint32_t GetTraceId () const;
  /**
   * \brief Set the trace id of corresponding unit flow.
   */
  void SetTraceId (uint32_t traceId);
  /**
   * \brief Increase the usage by the size of new packet.
   */
  void AddUsage (uint32_t pktSize);

private:
  uint32_t m_flowId; //!< The local id of this flow
  uint32_t m_traceId; //! The id used in tracing, corresponding to the trace id of unit flow
  TracedValue<DataRate> m_rate; //!< The configured rate
  TracedValue<double> m_usage; //!< The usage in bytes
};

/**
 * \ingroup bandwidth-manager
 *
 * \brief A multi-class token bucket queue discipline for BwM project
 * 
 * This qdisc uses BwmQueueDiscClass as its QueueDiscClass
 */
class BwmQueueDisc : public QueueDisc {
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  /**
   * \brief BwmQueueDisc constructor
   */
  BwmQueueDisc ();

  virtual ~BwmQueueDisc ();

  /**
   * \brief Set up the local agent linked to this queue disc
   */
  void SetupLocalAgent (Ptr<BwmLocalAgent> agent);

  /**
   * \brief Set the number of flow queues
   */
  void SetFlowNum (uint32_t flowNum);

private:
  virtual bool DoEnqueue (Ptr<QueueDiscItem> item);
  virtual Ptr<QueueDiscItem> DoDequeue (void);
  virtual bool CheckConfig (void);
  virtual void InitializeParams (void);

  /**
   * \brief Drop packets to ensure queue limit
   */
  void Drop (void);

  Ptr<BwmLocalAgent> m_agent; //!< The pointer recording the local agent that controls this Bwm Queue Disc

  std::map<uint32_t, uint32_t> m_flowNumIndices; //!< The mapping from
  uint32_t m_flowNum; //!< The maximum number of QueueDiscClass
  uint32_t m_nextFlow; //!< The index of the flow that is about dequeue an item

  TracedCallback<Ptr<BwmQueueDiscClass> > m_flowCreateTrace; //!< Trace of creating internal queue disc class

  ObjectFactory m_queueDiscClassFactory; //!< Factory to create a new internal queue disc class
  ObjectFactory m_queueDiscFactory;	//!< Factory to create a new internal child queue disc
  std::string m_queueDiscClassTypeId; //!< TypeId of the internal queue disc classs
  std::string m_queueDiscTypeId; //!< TypeId of the internal queue disc
};

}

#endif
