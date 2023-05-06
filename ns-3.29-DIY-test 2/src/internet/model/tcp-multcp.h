#ifndef TCPMULTCP_H
#define TCPMULTCP_H

#include "ns3/tcp-socket-state.h"
#include "ns3/tcp-congestion-ops.h"

namespace ns3 {

class TcpMultcp : public TcpNewReno
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  TcpMultcp ();

  /**
   * \brief Copy constructor.
   * \param sock object to copy.
   */
  TcpMultcp (const TcpMultcp& sock);

  ~TcpMultcp () {}

  virtual void IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);
  virtual uint32_t GetSsThresh (Ptr<const TcpSocketState> tcb,
                                uint32_t bytesInFlight);

  virtual Ptr<TcpCongestionOps> Fork ();
  virtual std::string GetName () const;

protected:
  virtual uint32_t SlowStart (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);
  virtual void CongestionAvoidance (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);
};

} // namespace ns3

#endif