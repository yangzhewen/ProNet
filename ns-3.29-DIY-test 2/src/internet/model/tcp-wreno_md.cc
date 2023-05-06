#include "tcp-wreno_md.h"
#include "tcp-socket-base.h"
#include "ns3/log.h"
#include <math.h>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpWrenoMD");
NS_OBJECT_ENSURE_REGISTERED (TcpWrenoMD);

TcpWrenoMD::TcpWrenoMD () : TcpNewReno() { NS_LOG_FUNCTION(this); }

TcpWrenoMD::TcpWrenoMD (const TcpWrenoMD& sock) : TcpNewReno(sock)
{
  NS_LOG_FUNCTION (this);
}

TypeId
TcpWrenoMD::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::TcpWrenoMD")
    .SetParent<TcpCongestionOps> ()
    .SetGroupName ("Internet")
    .AddConstructor<TcpWrenoMD> ()
  ;

  return tid;
}

void
TcpWrenoMD::IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
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
TcpWrenoMD::GetSsThresh (Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight)
{
  NS_LOG_FUNCTION (this << tcb << bytesInFlight);

  uint32_t newcWnd = bytesInFlight * (1 - 0.5 / tcb->m_weight);
  NS_LOG_INFO ("In GetSsThresh, update cwnd to " << std::max (2 * tcb->m_segmentSize, newcWnd) << " bytesInFlight " << bytesInFlight);

  return std::max (2 * tcb->m_segmentSize, newcWnd);
}

uint32_t
TcpWrenoMD::SlowStart (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked);

  if (segmentsAcked >= 1)
    {
      /* Original Code */
      //tcb->m_cWnd += 1 * tcb->m_segmentSize;

      tcb->m_cWnd += 1 * tcb->m_segmentSize;
      tcb->m_preciseCWnd = tcb->m_cWnd;
      NS_LOG_INFO ("In SlowStart, update cwnd to " << tcb->m_cWnd << " ssthresh " << tcb->m_ssThresh);
      return segmentsAcked - 1;
    }

  return 0;
}

void
TcpWrenoMD::CongestionAvoidance (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked);

  if (segmentsAcked > 0)
    {
      /* Original Code */
      // double adder = static_cast<double> (1 * tcb->m_segmentSize * tcb->m_segmentSize) / tcb->m_cWnd.Get ();
      // adder = std::max (1.0, adder);
      // tcb->m_cWnd += static_cast<uint32_t> (adder);

      if (fabs(tcb->m_preciseCWnd - tcb->m_cWnd) > 1.0)
        {//indicating inconsistency
          tcb->m_preciseCWnd = tcb->m_cWnd;
        }
      double adder = static_cast<double> (1 * tcb->m_segmentSize * tcb->m_segmentSize) / tcb->m_cWnd.Get ();
      tcb->m_preciseCWnd += adder;
      tcb->m_cWnd = static_cast<uint32_t> (tcb->m_preciseCWnd);
      NS_LOG_INFO ("In CongAvoid, update cwnd to " << tcb->m_cWnd <<
                   " ssthresh " << tcb->m_ssThresh);
    }
}

Ptr<TcpCongestionOps>
TcpWrenoMD::Fork (void)
{
  NS_LOG_FUNCTION (this);

  return CopyObject<TcpWrenoMD> (this);
}

std::string
TcpWrenoMD::GetName () const
{
  NS_LOG_FUNCTION (this);

  return "TcpWrenoMD";
}

}
