#include "tcp-ewtcp.h"
#include "tcp-socket-base.h"
#include "ns3/log.h"
#include <math.h>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpEwtcp");
NS_OBJECT_ENSURE_REGISTERED (TcpEwtcp);

TcpEwtcp::TcpEwtcp () : TcpNewReno() { NS_LOG_FUNCTION(this); }

TcpEwtcp::TcpEwtcp (const TcpEwtcp& sock) : TcpNewReno(sock)
{
  NS_LOG_FUNCTION (this);
}

TypeId
TcpEwtcp::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::TcpEwtcp")
    .SetParent<TcpCongestionOps> ()
    .SetGroupName ("Internet")
    .AddConstructor<TcpEwtcp> ()
  ;

  return tid;
}

void
TcpEwtcp::IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
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
TcpEwtcp::GetSsThresh (Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight)
{
  NS_LOG_FUNCTION (this << tcb << bytesInFlight);
  NS_LOG_INFO("Flow weight: " << tcb->m_weight);

  uint32_t newcWnd = 0; //bytes
  newcWnd = bytesInFlight / 2.0;

  return std::max (2 * tcb->m_segmentSize, newcWnd);
}

uint32_t
TcpEwtcp::SlowStart (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked);
  NS_LOG_INFO("Flow weight: " << tcb->m_weight);

  if (segmentsAcked >= 1)
    {
      tcb->m_cWnd += 1 * tcb->m_segmentSize;
      NS_LOG_INFO ("In SlowStart, updated to cwnd " << tcb->m_cWnd << " ssthresh " << tcb->m_ssThresh);
      return segmentsAcked - 1;
    }

  return 0;
}

void
TcpEwtcp::CongestionAvoidance (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked);
  NS_LOG_INFO("Flow weight: " << tcb->m_weight);

  if (segmentsAcked > 0)
    {
      double adder = static_cast<double> (pow(tcb->m_weight, 2) * tcb->m_segmentSize * tcb->m_segmentSize) / tcb->m_cWnd.Get ();
      adder = std::max (1.0, adder);
      tcb->m_cWnd += static_cast<uint32_t> (adder);
      NS_LOG_INFO ("In CongAvoid, updated to cwnd " << tcb->m_cWnd <<
                   " ssthresh " << tcb->m_ssThresh);
    }
}

Ptr<TcpCongestionOps>
TcpEwtcp::Fork (void)
{
  NS_LOG_FUNCTION (this);

  return CopyObject<TcpEwtcp> (this);
}

std::string
TcpEwtcp::GetName () const
{
  NS_LOG_FUNCTION (this);

  return "TcpEwtcp";
}

}
