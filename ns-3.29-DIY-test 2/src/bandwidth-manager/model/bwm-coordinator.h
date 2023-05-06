#ifndef BWM_COORD_H
#define BWM_COORD_H

#include "ns3/object.h"
#include "ns3/object-factory.h"
#include "ns3/traced-value.h"
#include "ns3/application.h"

#include <list>
#include <map>

namespace ns3 {

class BandwidthFunction;
class BwmLocalAgent;

/**
 * \ingroup bandwidth-manager
 *
 * \brief A class describing unit flows in the bandwidth-manager system
 * 
 * A unit flow is a set of flows that act as 
 * a distributed unit of a tenant in the bandwidth-manager system
 */
class UnitFlow : public Object {
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  /**
   * \brief UnitFlow constructor.
   */
  UnitFlow ();

  ~UnitFlow ();

  /**
   * \brief Get the configured bandwidth function.
   * \return The smart pointer to the target BF.
   */
  Ptr<BandwidthFunction> GetConfiguredBF ();
  /**
   * \brief Set the configured bandwidth function of the unit flow.
   * 
   * This method doesn't check whether there exists an old bandwidth function.
   */
  void SetConfiguredBF (Ptr<BandwidthFunction>);
  /**
   * \brief Get the transformed bandwidth function.
   * \return The smart pointer to the target BF.
   */
  Ptr<BandwidthFunction> GetTransformedBF ();
  /**
   * \brief Set the transformed bandwidth function of the unit flow.
   * 
   * This method doesn't check whether there exists an old bandwidth function.
   */
  void SetTransformedBF (Ptr<BandwidthFunction>);
  /**
   * \brief Get the id of the tenant that this unit flow belongs to.
   * \return m_tenatId.
   */
  uint32_t GetTenantId () const;
  /**
   * \brief Set the id of the tenant that this unit flow belongs to.
   */
  void SetTenantId (uint32_t tenantId);
  /**
   * \brief Get the unique id of this unit flow.
   * \return m_flowId.
   */
  uint32_t GetFlowId () const;
  /**
   * \brief Set the id of the unit flow.
   */
  void SetFlowId (uint32_t flowId);
  /**
   * \brief Get the unique trace id of this unit flow.
   * \return m_traceId.
   */
  uint32_t GetTraceId () const;
  /**
   * \brief Set the trace id of the unit flow.
   */
  void SetTraceId (uint32_t traceId);
  /**
   * \brief Set the bandwidth usage in last interval of this unit flow.
   */
  void SetBandwidthUsage (double calculatedUsage);
  /**
   * \brief Set the allocated fair share of this unit flow.
   */
  void SetAllocatedFS (double fairShare);
  /**
   * \brief Set the congestion factor of this unit flow.
   */
  void SetCongestionFactor (double factor);

  /**
    * \Return the bandwidth usage of this unit flow.
    */
  double GetBandwidthUsage(void) const;
  /**
   * \brief Get the allocated fair share of this unit flow.
   * \return the allocated fair share
   */
  double GetAllocatedFS (void) const;
  /**
   * \brief Use the transformed bandwidth function to compute the allocated rate .
   */
  double GetAllocatedRate (void) const;
  /**
   * \brief Get the congestion factor of this unit flow.
   * \return the congestion factor
   */
  double GetCongestionFactor (void) const;

private:
  uint32_t m_traceId; //!< The flow id used in tracing
  uint32_t m_flowId; //!< Flow id
  uint32_t m_tenantId; //!< Id of the according tenant
  Ptr<BandwidthFunction> m_configuredBF; //!< Configured bandwidth function
  Ptr<BandwidthFunction> m_transformedBF; //!< The effective bandwidth function

  TracedValue<double> m_usage; //!< Latest bandwidth usage of this unit flow
  TracedValue<double> m_allocatedFS; //!< The allocated fair share of this unit flow
  TracedValue<double> m_congestionFactor; //!< The measured congestion condition of this unit flow
};

/**
 * \ingroup bandwidth-manager
 *
 * \brief A class describing tenants in the bandwidth-manager system
 * 
 * A tenant if an aggregate entity in the bandwidth-manager system.
 * It consists of several unit flows.
 */
class Tenant : public Object {
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  /**
   * \brief Tenant constructor.
   */
  Tenant ();

  ~Tenant ();

  /**
   * \brief Get the id of the tenant.
   * \return m_tenatId.
   */
  uint32_t GetTenantId () const;
  /**
   * \brief Set the id of the tenant.
   */
  void SetTenantId (uint32_t tenantId);
  /**
   * \brief Set the bandwidth function of the tenant.
   * 
   * This method analyzes the string and configure a bandwidth function to the tenant.
   * It doesn't check whether there exists an old bandwidth function.
   */
  void SetBF (std::string bfStr);
  /**
   * \brief Set the host weight table of the tenant.
   * 
   * This method analyzes the string and configure a host weight table to the tenant.
   */
  void SetHostWeightTable (std::string entryListStr);
  /**
   * \brief Get the actual fair share of the tenant.
   * \return the actual fair share.
   */
  double GetActualFS ();
  /**
   * \brief Get the weight of host denoted by hostId from the tenant's perspective.
   * \return the weight of specific host.
   */
  double GetHostWeight (uint32_t hostId);
  /**
   * \brief Transform bandwidth functions of unit flows attached to the tenant.
   * 
   * This method implements the bandwidth function transformation algorithm
   * proposed in BwE (Alok Kumar et al., SIGCOMM'15).
   */
  void TransformComponentialBF ();
  /**
   * \brief Add a new unit flow into the flow table.
   */
  void AddUnitFlow (Ptr<UnitFlow>);
  /**
   * \brief Update the status of a unit flow, including usage, etc.
   */
  void UpdateUnitFlow (Ptr<UnitFlow>);

private:
  uint32_t m_tenantId; //!< Tenant Id
  std::map<uint32_t, Ptr<UnitFlow> > m_flowTable; //!< The flow mapping table consisting of all attached unit flows, flowId -> flow
  std::map<uint32_t, double> m_hostWeightTable; //!< The weight table that assign a weight to each host
  Ptr<BandwidthFunction> m_BF; //!< Configured bandwidth function

  TracedValue<double> m_actualFairShare; //!< The actual fair share derived from usage reports
};

/**
 * \ingroup bandwidth-manager
 *
 * \brief A class describing the central coordinator in the bandwidth-manager system
 * 
 * The coordinator in the bandwidth-manager system takes charge of
 * transforming of local unit flows,
 * computing new optimization arguments,
 * etc.
 */
class BwmCoordinator : public Application {
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  /**
   * \brief BwmCoordinator constructor.
   */
  BwmCoordinator ();

  ~BwmCoordinator ();

  /**
   * \brief Input the global configuration of tenants from a file.
   * 
   * The input format should be p1,q1 p2,q2 ...
   * (p, q)s are just points of the tenant's bandwidth function 
   */
  void InputConfiguration (std::string filePath);
  /**
   * \brief Register a new host by submitting its local agent.
   * \return true if the operation succeed, false otherwise.
   */
  bool RegisterHost (Ptr<BwmLocalAgent> host);
  /**
   * \brief Register a new unit flow.
   * \return the pointer to flow if the registration succeeds, NULL otherwise.
   */
  Ptr<UnitFlow> RegisterFlow (uint32_t tenantId, uint32_t flowId, uint32_t traceId, std::string extraInfo);
  /**
   * \brief Update the usage information of tenants according to reports from a host.
   */
  void UpdateUsage (Ptr<BwmLocalAgent> host, std::list<Ptr<UnitFlow> > flowList);

private:
  /**
   * \brief Start up the application.
   */
  void StartApplication (void);
  /**
   * \brief Shut down the application.
   */
  void StopApplication (void);
  /**
   * \brief Automatically configure a bandwidth functions for a new unit flow
   * 
   * This method will configure the bandwidth function according to the extra information
   * The default configuration is BF = min (srcWeight + dstWeight, deviceRateLimit)
   */
  void AutoConfigureBF (Ptr<UnitFlow> flow, std::string extraInfo);
  /**
   * \brief Estimate the new target status based on fresh usage reports.
   * \return The new estimated target status, ie a new fair share shared by all tenants
   */
  double EstimateTargetStatus ();
  /**
   * \brief Disseminate new arguments that should be used on hosts.
   */
  void SendNewArguments (Ptr<BwmLocalAgent> targetHost, double fairshare);

  double m_alpha; //!< The progress factor of Target Status Estimation Algorithm, within [0, 1)
  double m_minFS; //!< Lower bound of the fair share of the entire system

  std::map<uint32_t, Ptr<Tenant> > m_tenantTable; //!< Global tenant mapping table, tenantId -> tenant
  std::list<Ptr<BwmLocalAgent> > m_hostList; //!< List of local agents in all hosts.
  uint32_t m_hostCounter; //!< The monotonously increasing counter of hosts used to assign id for new hosts

  ObjectFactory m_flowFactory; //!< The object factory used to create unit flows
  ObjectFactory m_tenantFactory; //!< The object factory used to create tenants

  TracedCallback<Ptr<Tenant> > m_tenantCreateTrace; //!< Trace of creating a tenant
  TracedCallback<Ptr<UnitFlow> > m_unitFlowCreateTrace; //!< Trace of creating a unit flow
};

}

#endif
