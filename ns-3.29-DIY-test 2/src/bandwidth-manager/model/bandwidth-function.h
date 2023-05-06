#ifndef BANDWIDTH_FUNCTION_H
#define BANDWIDTH_FUNCTION_H

#include "ns3/object.h"
#include <vector>
#include <iostream>

namespace ns3 {

/**
 * \ingroup bandwidth-manager
 *
 * \brief A class describing bandwidth functions
 * 
 * A bandwidth function is a linear monotonically increasing fucntion
 * that map fair share to bandwidth
 */
class BandwidthFunction : public Object {
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  /**
   * \brief Bandwidth constructor
   */
  BandwidthFunction ();

  virtual ~BandwidthFunction ();

  /**
   * \brief Map the fair share to bandwidth.
   * \return the exact bandwidth corresponding to the fair share
   */
  double GetBandwidth (double fairShare) const;
  /**
   * \brief Map the bandwidth to fair share.
   * \return the minimum fair share that can reach the bandwidth
   */
  double GetFairShare (double bandwidth) const;
  /**
   * \brief Add a new vertex to this bandwidth function.
   * \return true if the operation succeeds, false otherwise
   */
  bool AddVertex (double fairShare, double bandwidth);
  /**
   * \brief Analyze the function and get fair share of the nearest interesting point behind the argument on the X axe.
   * \return if there exists an interesting point, the fair share of the next interesting point, otherwise -1.
   */
  double GetNextInterestingPointByFS (double currentFairShare) const;
  /**
   * \brief Analyze the function and get the nearest interesting point above the argument on the Y axe.
   * \return if there exists an interesting point, the bandwidth of the next interesting point, otherwise -1.
   */
  double GetNextInterestingPointByBW (double currentBandwidth) const;
  /**
   * \brief Overload the output operator to print bandwidth function.
   * \return The output stream.
   */
  friend std::ostream& operator << (std::ostream& out, const BandwidthFunction& bf);

  static constexpr double INF = -1;
private:
  std::vector<std::pair<double, double>> m_vertexTable; //!< the vertex table that records each non-trivival point 
};

}

#endif
