#include "tcp-multcp.h"
#include "tcp-socket-base.h"
#include "ns3/log.h"
#include <math.h>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpMultcp");
NS_OBJECT_ENSURE_REGISTERED (TcpMultcp);

TcpMultcp::TcpMultcp () : TcpNewReno() { NS_LOG_FUNCTION(this); }

TcpMultcp::TcpMultcp (const TcpMultcp& sock) : TcpNewReno(sock)
{
  NS_LOG_FUNCTION (this);
}

TypeId
TcpMultcp::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::TcpMultcp")
    .SetParent<TcpCongestionOps> ()
    .SetGroupName ("Internet")
    .AddConstructor<TcpMultcp> ()
  ;

  return tid;
}

void
TcpMultcp::IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
    NS_LOG_FUNCTION (this << tcb << segmentsAcked);

  if (tcb->m_cWnd < tcb->m_ssThresh)
    {
      segmentsAcked = SlowStart (tcb, segmentsAcked);
    }

  if (tcb->m_cWnd >= tcb->m_ssThresh)
    {
      CongestionAvoidance (tcb, segmentsAcked);
    }
}
uint32_t
TcpMultcp::GetSsThresh (Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight)
{
  NS_LOG_FUNCTION (this << tcb << bytesInFlight);
  NS_LOG_INFO("Flow weight: " << tcb->m_weight);

  uint32_t newcWnd = 0; //bytes
  if (tcb->m_cWnd < tcb->m_ssThresh)
    newcWnd = bytesInFlight / 2.0;
  else
    newcWnd = bytesInFlight * ((tcb->m_weight - 0.5) / tcb->m_weight);

  return std::max (2 * tcb->m_segmentSize, newcWnd);
}

uint32_t
TcpMultcp::SlowStart (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked);
  NS_LOG_INFO("Flow weight: " << tcb->m_weight);

  if (segmentsAcked >= 1)
    {
      if (tcb->m_cWnd <= pow(3.0, log(tcb->m_weight) / (log(3) - log(2))))
        tcb->m_cWnd += 2 * tcb->m_segmentSize;
      else
        tcb->m_cWnd += 1 * tcb->m_segmentSize;
      NS_LOG_INFO ("In SlowStart, updated to cwnd " << tcb->m_cWnd << " ssthresh " << tcb->m_ssThresh);
      return segmentsAcked - 1;
    }

  return 0;
}

void
TcpMultcp::CongestionAvoidance (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked);
  NS_LOG_INFO("Flow weight: " << tcb->m_weight);

  if (segmentsAcked > 0)
    {
      double adder = static_cast<double> (tcb->m_weight * tcb->m_segmentSize * tcb->m_segmentSize) / tcb->m_cWnd.Get ();
      adder = std::max (1.0, adder);
      tcb->m_cWnd += static_cast<uint32_t> (adder);
      NS_LOG_INFO ("In CongAvoid, updated to cwnd " << tcb->m_cWnd <<
                   " ssthresh " << tcb->m_ssThresh);
    }
}

Ptr<TcpCongestionOps>
TcpMultcp::Fork (void)
{
  NS_LOG_FUNCTION (this);

  return CopyObject<TcpMultcp> (this);
}

std::string
TcpMultcp::GetName () const
{
  NS_LOG_FUNCTION (this);

  return "TcpMultcp";
}

}
