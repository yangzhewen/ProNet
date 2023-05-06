#include "ns3/log.h"

#include "bandwidth-function.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BandwidthFunction");

NS_OBJECT_ENSURE_REGISTERED (BandwidthFunction);

TypeId
BandwidthFunction::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::BandwidthFunction")
    .SetParent<Object> ()
    .SetGroupName ("BandwidthManager")
    .AddConstructor<BandwidthFunction> ()
  ;

  return tid;
}

BandwidthFunction::BandwidthFunction ()
{
  AddVertex (0, 0);
}

BandwidthFunction::~BandwidthFunction ()
{

}

double
BandwidthFunction::GetBandwidth (double fairShare) const
{
  // meet INF, return the upper bound
  if (fairShare == BandwidthFunction::INF)
    {
      return m_vertexTable.back ().second;
    }

  auto it = m_vertexTable.begin ();
  while (it != m_vertexTable.end ())
  {
    // compare the fair share of current point to the argument
    if (it->first == fairShare)
      {
        auto nextIt = it + 1;
        // check whether there exist two points with the same fair share
        if (nextIt != m_vertexTable.end () && nextIt->first == it->first)
          {
            // the second point exists, use the bandwidth of second point
            return nextIt->second;
          }
        else
          {
            // no second point, use the original bandwidth
            return it->second;
          }
      }
    else
      {
        // notice that the fair share of current point must be not greater than the argument
        NS_ASSERT (it->first < fairShare);

        auto nextIt = it + 1;
        // check whether there exists a next point
        if (nextIt != m_vertexTable.end ())
          {
            // a next point exists, check whether it is located in section [it->first, nextIt->first)
            if (fairShare < nextIt->first)
              {
                // the target point denoted by the argument is located in this section
                // compute the bandwidth value of argument using two endpoints of this section
                double result = it->second + ((fairShare - it->first) / (nextIt->first - it->first)) * (nextIt->second - it->second);
                return result;
              }
          }
        else
          {
            // no next point, use the bandwidth of current point
            return it->second;
          }
      }
    
    it++;
  }
  
  return 0.0;
}

double
BandwidthFunction::GetFairShare (double bandwidth) const
{
  // meet INF, return INF, indicate that there is no corresponding fairShare
  if (bandwidth == BandwidthFunction::INF)
    {
      return BandwidthFunction::INF;
    }

  auto it = m_vertexTable.begin ();
  while (it != m_vertexTable.end ())
  {
    // compare the bandwidth of current point to the argument
    if (it->second == bandwidth)
      {
        return it->first;
      }
    else
      {
        // notice that the bandwidth of current point must be not greater than the argument
        NS_ASSERT (it->second < bandwidth);

        auto nextIt = it + 1;
        // check whether there exists a next point
        if (nextIt != m_vertexTable.end ())
          {
            // a next point exists, check whether it is located in section [it->second, nextIt->second)
            if (bandwidth < nextIt->second)
              {
                // the target point denoted by the argument is located in this section
                // compute the bandwidth value of argument using two endpoints of this section
                double result = it->first + ((bandwidth - it->second) / (nextIt->second - it->second)) * (nextIt->first - it->first);
                return result;
              }
          }
        else
          {
            // no next point, the fair share corresponding to the argument doesn't exist
            return BandwidthFunction::INF;
          }
      }
    
    it++;
  }

  return 0.0;
}

double
BandwidthFunction::GetNextInterestingPointByBW (double currentBandwidth) const
{
  for (auto it : m_vertexTable)
    {
      if (it.second > currentBandwidth)
        {
          // find the point that is just above current bandwidth
          return it.second;
        }
    }

  // no next interesting point
  return BandwidthFunction::INF;
}

double
BandwidthFunction::GetNextInterestingPointByFS (double currentFairShare) const
{
  for (auto it : m_vertexTable)
    {
      if (it.first > currentFairShare)
        {
          // find the point that is just behind current fair share.
          return it.first;
        }
    }

  // no next interesting point
  return BandwidthFunction::INF;
}

bool
BandwidthFunction::AddVertex (double fairShare, double bandwidth)
{
  if (m_vertexTable.empty () == false && bandwidth < m_vertexTable.back ().second) return false;

  auto newVertex = std::make_pair (fairShare, bandwidth);
  m_vertexTable.push_back (newVertex);
  return true;
}

std::ostream&
operator << (std::ostream& out, const BandwidthFunction& bf)
{
  for (auto pair : bf.m_vertexTable)
    {
      out << pair.first << "," << pair.second << " ";
    }
  return out;
}

}
