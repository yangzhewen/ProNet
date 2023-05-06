#include "ns3/log.h"
#include "ns3/string.h"
#include "ns3/double.h"

#include "bwm-coordinator.h"
#include "bwm-local-agent.h"
#include "bandwidth-function.h"

#include <sstream>
#include <fstream>
#include <queue>
#include <functional>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BwmCoordinator");

NS_OBJECT_ENSURE_REGISTERED (UnitFlow);

TypeId
UnitFlow::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::UnitFlow")
    .SetParent<Object> ()
    .SetGroupName ("BandwidthManager")
    .AddConstructor<UnitFlow> ()
    .AddTraceSource ("AllocatedFairShare",
                     "Real time allocated fair share of the unit flow",
                     MakeTraceSourceAccessor (&UnitFlow::m_allocatedFS),
                     "ns3::TracedValueCallback::Double")
    .AddTraceSource ("Usage",
                     "Statistical bandwidth usage",
                     MakeTraceSourceAccessor (&UnitFlow::m_usage),
                     "ns3::TracedValueCallback::Double")
  ;
  return tid;
}

UnitFlow::UnitFlow ()
  : m_traceId (0),
    m_flowId (0),
    m_tenantId (0),
    m_configuredBF (NULL),
    m_transformedBF (NULL),
    m_usage (0),
    m_allocatedFS (0),
    m_congestionFactor (0)
{
  
}

UnitFlow::~UnitFlow ()
{

}

Ptr<BandwidthFunction>
UnitFlow::GetConfiguredBF ()
{
  return m_configuredBF;
}

void
UnitFlow::SetConfiguredBF (Ptr<BandwidthFunction> configuredBF)
{
  m_configuredBF = configuredBF;
}

Ptr<BandwidthFunction>
UnitFlow::GetTransformedBF ()
{
  return m_transformedBF;
}

void
UnitFlow::SetTransformedBF (Ptr<BandwidthFunction> transformedBF)
{
  NS_LOG_UNCOND ("Trans TBF: " << *transformedBF << " for flow " << m_traceId);
  m_transformedBF = transformedBF;
}

uint32_t
UnitFlow::GetTenantId () const
{
  return m_tenantId;
}

void
UnitFlow::SetTenantId (uint32_t tenantId)
{
  m_tenantId = tenantId;
}

uint32_t
UnitFlow::GetFlowId () const
{
  return m_flowId;
}

void
UnitFlow::SetFlowId (uint32_t flowId)
{
  m_flowId = flowId;
}

uint32_t
UnitFlow::GetTraceId () const
{
  return m_traceId;
}

void
UnitFlow::SetTraceId (uint32_t traceId)
{
  m_traceId = traceId;
}

void
UnitFlow::SetBandwidthUsage (double calculatedUsage)
{
  m_usage = calculatedUsage;
}

double
UnitFlow::GetBandwidthUsage (void) const
{
  return m_usage;
}

void
UnitFlow::SetAllocatedFS (double fairShare)
{
  m_allocatedFS = fairShare;
}

double
UnitFlow::GetAllocatedFS (void) const
{
  return m_allocatedFS;
}

void
UnitFlow::SetCongestionFactor (double factor)
{
  m_congestionFactor = factor;
}

double
UnitFlow::GetCongestionFactor (void) const
{
  return m_congestionFactor;
}

double
UnitFlow::GetAllocatedRate (void) const
{
  if (m_transformedBF == NULL)
    {
      NS_LOG_INFO ("The flow hasn't been registered!");
      return 0;
    }
  return m_transformedBF->GetBandwidth (m_allocatedFS);
}

NS_OBJECT_ENSURE_REGISTERED (Tenant);

TypeId
Tenant::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::Tenant")
    .SetParent<Object> ()
    .SetGroupName ("BandwidthManager")
    .AddConstructor<Tenant> ()
    .AddTraceSource ("ActualFairShare",
                     "Actual fair share of the tenant",
                     MakeTraceSourceAccessor (&Tenant::m_actualFairShare),
                     "ns3::TracedValueCallback::Double")
  ;
  return tid;
}

Tenant::Tenant ()
  : m_tenantId (0),
    m_BF (NULL),
    m_actualFairShare (0)
{

}

Tenant::~Tenant ()
{

}

uint32_t
Tenant::GetTenantId () const
{
  return m_tenantId;
}

void
Tenant::SetTenantId (uint32_t tenantId)
{
  m_tenantId = tenantId;
}

void
Tenant::SetBF (std::string bfStr)
{
  std::istringstream sin (bfStr);

  // analyze the bandwidth function of new tenant
  Ptr<BandwidthFunction> newBF (new BandwidthFunction);
  std::string pointPair;
  while (sin >> pointPair)
    {
      // extract the info of point
      int split = pointPair.find (',');
      std::string fairShare = pointPair.substr (0, split);
      std::string bandwidth = pointPair.substr (split + 1, pointPair.length ());
      if (fairShare.length () == 0 || bandwidth.length () == 0)
        {
          NS_LOG_WARN ("Invalid input format!");
          NS_ASSERT (0);
        }
      
      // add a new point to the bandwidth function of new tenant
      newBF->AddVertex (atof (fairShare.c_str ()), atof (bandwidth.c_str ()));
    }

  m_BF = newBF;
}

void
Tenant::SetHostWeightTable (std::string entryListStr)
{
  std::istringstream sin (entryListStr);

  std::string hwPair;
  while (sin >> hwPair)
    {
      // extract the info of host-weight pair
      int split = hwPair.find (',');
      std::string hostId = hwPair.substr (0, split);
      std::string weight = hwPair.substr (split + 1, hwPair.length ());
      if (hostId.length () == 0 || weight.length () == 0)
        {
          NS_LOG_WARN ("Invalid input format!");
          NS_ASSERT (0);
        }
      
      // add a new entry to the host weight table of new tenant
      m_hostWeightTable.insert (std::make_pair (atoi (hostId.c_str ()), atof (weight.c_str ())));
    }
}

double
Tenant::GetActualFS ()
{
  double usageSum=0;
  for (auto it : m_flowTable)
    {
      usageSum += it.second->GetBandwidthUsage();
    }
  m_actualFairShare = m_BF->GetFairShare(usageSum);
  return m_actualFairShare;
}

double
Tenant::GetHostWeight (uint32_t hostId)
{
  if (m_hostWeightTable.find (hostId) == m_hostWeightTable.end ())
    {
      return 1.0;
    }
  else
    {
      return m_hostWeightTable[hostId];
    }
}

void
Tenant::TransformComponentialBF ()
{
  // collect bandwidth functions of all unit flows
  std::vector<Ptr<BandwidthFunction> > BFArray;
  for (auto flow : m_flowTable)
    {
      BFArray.push_back (flow.second->GetConfiguredBF ());
    }

  // compute the aggregated bandwidth function

  Ptr<BandwidthFunction> aggregateBF (new BandwidthFunction);
  /// initialize the heap of interesting points
  auto BFCmp = [] (std::pair<double, int> left, std::pair<double, int> right) -> bool { return left.first < right.first; };
  /* std::priority_queue<std::pair<double, int>, std::vector<std::pair<double, int>>, BC_Cmp1> interestingPointsList; */
  std::priority_queue<std::pair<double, int>, std::vector<std::pair<double, int>>, std::function<bool (std::pair<double, int> left, std::pair<double, int> right)>> interestingPointsList(BFCmp);
  int counter = 0;
  for (auto bf : BFArray)
    {
      if (bf->GetNextInterestingPointByFS (0) > 0)
        {
          interestingPointsList.push (std::make_pair (bf->GetNextInterestingPointByFS (0), counter));
        }
      counter++;
    }
  
  /// check each interesting point and compute aggregated bandwidth value for them
  while (interestingPointsList.empty () != true)
    {
      double minPoint = interestingPointsList.top ().first;
      //// sum bandwidth values of all bandwith functions at the min point
      double sum = 0.0;
      for (auto bf : BFArray)
        {
          sum += bf->GetBandwidth (minPoint);
        }

      aggregateBF->AddVertex (minPoint, sum);

      //// get next interesting points of the bandwidth functions that have the minPoint
      //// and use them to replace minPoints in the heap
      while (interestingPointsList.empty () != true && interestingPointsList.top ().first == minPoint)
        {
          int targetBF = interestingPointsList.top ().second;
          interestingPointsList.pop ();
          if (BFArray[targetBF]->GetNextInterestingPointByFS (minPoint) > 0)
            {
              interestingPointsList.push (std::make_pair (BFArray[targetBF]->GetNextInterestingPointByFS (minPoint), targetBF));
            }
        }
    }

  // build a map to represent the transformation function
  // it maps a small fair share in the aggregate bandwidth function
  // to a larger fair share in the configured bandwidth function
  std::vector<std::pair<double, double> > transformMap;
  double currentBW = 0.0;
  auto FpEqual = [](double left, double right) -> bool { return fabs (left - right) < 1e-3; };
  while (!FpEqual (currentBW, -1))
    {
      double p1 = aggregateBF->GetNextInterestingPointByBW (currentBW);
      double p2 = m_BF->GetNextInterestingPointByBW (currentBW);
      double minPoint = std::min (p1, p2);
      /// if minPoint equals to -1, check whether there exists a hidden interesting point
      if (FpEqual (minPoint, -1) && !FpEqual (p1, -1))
        {
          minPoint = p1;
        }
      else if (FpEqual (minPoint, -1) && !FpEqual (p2, -1))
        {
          minPoint = p2;
        }
      else if (FpEqual (minPoint, -1))
        {
          break;
        }
      if (aggregateBF->GetFairShare (minPoint) != BandwidthFunction::INF && m_BF->GetFairShare (minPoint) != BandwidthFunction::INF)
        {
          transformMap.push_back (std::make_pair (aggregateBF->GetFairShare (minPoint), m_BF->GetFairShare (minPoint)));
          currentBW = minPoint;
        }
      else
        {
          currentBW = BandwidthFunction::INF;
        }
    }

  // transform each unit flow's bandwidth function
  for (auto flow : m_flowTable)
    {
      Ptr<BandwidthFunction> transformedBF (new BandwidthFunction);
      Ptr<BandwidthFunction> configuredBF = flow.second->GetConfiguredBF ();
      for (auto vertex : transformMap)
        {
          transformedBF->AddVertex (vertex.second, configuredBF->GetBandwidth (vertex.first));
        }
      flow.second->SetTransformedBF (transformedBF);
    }
}

void
Tenant::AddUnitFlow (Ptr<UnitFlow> flow)
{
  NS_LOG_UNCOND ("Add flow " << flow->GetFlowId () << " trace id " << flow->GetTraceId ());
  m_flowTable.insert (std::make_pair (flow->GetFlowId (), flow));
}

void
Tenant::UpdateUnitFlow (Ptr<UnitFlow> targetFlow)
{
  uint32_t flowId = targetFlow->GetFlowId ();

  // check whether this unit flow has been registered
  if (m_flowTable.find (flowId) == m_flowTable.end ())
    {
      // unregistered, warning
      NS_LOG_WARN ("Try to update unregistered unit-flow");
    }
  else
    {
      // registered, update this unit flow
      m_flowTable[flowId] = targetFlow;
    }

  // TODO: add time stamp
}

NS_OBJECT_ENSURE_REGISTERED (BwmCoordinator);

TypeId
BwmCoordinator::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::BwmCoordinator")
    .SetParent<Application> ()
    .SetGroupName ("BandwidthManager")
    .AddConstructor<BwmCoordinator> ()
    .AddAttribute ("ProgressFactor",
                   "The progress factor of Target Status Estimation Algorithm",
                   DoubleValue (0.1),
                   MakeDoubleAccessor (&BwmCoordinator::m_alpha),
                   MakeDoubleChecker<double> (0, 1))
    .AddAttribute ("MinFS",
                   "The minimum fair share of the whole system",
                   DoubleValue (3),
                   MakeDoubleAccessor (&BwmCoordinator::m_minFS),
                   MakeDoubleChecker<double> ())
    .AddTraceSource ("TenantCreate",
                     "Create a tenant",
                     MakeTraceSourceAccessor (&BwmCoordinator::m_tenantCreateTrace),
                     "ns3::BwmCoodinator::TenantTracedCallback")
    .AddTraceSource ("UnitFlowCreate",
                     "Create a unit-flow",
                     MakeTraceSourceAccessor (&BwmCoordinator::m_unitFlowCreateTrace),
                     "ns3::BwmQueueDisc::UnitFlowTracedCallback")                                 
  ;
  return tid;
}

BwmCoordinator::BwmCoordinator ()
{
  m_hostCounter = 0;
  m_flowFactory.SetTypeId (UnitFlow::GetTypeId ());
  m_tenantFactory.SetTypeId (Tenant::GetTypeId ());
}

BwmCoordinator::~BwmCoordinator ()
{

}

void
BwmCoordinator::StartApplication ()
{
  // pre-process
}

void
BwmCoordinator::StopApplication ()
{
  // post-process
}

void
BwmCoordinator::InputConfiguration (std::string filePath)
{
  std::ifstream fin (filePath);
  if (fin.fail ())
    {
      NS_LOG_WARN ("Cannot open tenant configuration file: " << filePath);
      exit (0);
    }

  std::string input;
  while (std::getline (fin, input))
    {
      if (input.length () > 0)
        {
          // meet a solid string, add a tenant
          Ptr<Tenant> newTenant = m_tenantFactory.Create<Tenant> ();
          uint32_t tenantId = atoi (input.c_str ());
          m_tenantTable.insert (std::make_pair (tenantId, newTenant));
          newTenant->SetTenantId (tenantId);

          // incorporate the bandwidth function to the new tenant
          std::getline (fin, input);
          if (input.length () > 0)
            {
              newTenant->SetBF (input);
            }
          else
            {
              NS_LOG_WARN ("Cannot find a bandwidth function for tenant: " << tenantId);
            }

          // read the next line and configure host weight table
          std::getline (fin, input);
          if (input.length () > 0)
            {
              newTenant->SetHostWeightTable (input);
            }
          else
            {
              NS_LOG_WARN ("Cannot find a host weight table for tenant: " << tenantId);
            }
          
          m_tenantCreateTrace (newTenant);
        }
      else
        {
          // meet a null string
          NS_LOG_INFO ("Meet a null line");
          break;
        }
    }
}

bool
BwmCoordinator::RegisterHost (Ptr<BwmLocalAgent> host)
{
  if (!host)
    {
      // pointer is null
      NS_LOG_INFO ("Cannot register an null host!");
      return false;
    }
  
  // insert the new host into the host list
  m_hostList.push_back (host);
  // assign host id
  host->SetHostId (m_hostCounter);
  m_hostCounter++;
  return true;
}

Ptr<UnitFlow>
BwmCoordinator::RegisterFlow (uint32_t tenantId, uint32_t flowId, uint32_t traceId, std::string extraInfo)
{
  if (m_tenantTable.find (tenantId) == m_tenantTable.end ())
    {
      NS_LOG_WARN ("Cannot find a tenant that matches such id: " << tenantId);
      return NULL;
    }

  Ptr<UnitFlow> flow = m_flowFactory.Create<UnitFlow> ();
  flow->SetTraceId (traceId);
  flow->SetFlowId (flowId);
  flow->SetTenantId (tenantId);
  AutoConfigureBF (flow, extraInfo);

  m_unitFlowCreateTrace (flow);

  return flow;
}

void
BwmCoordinator::AutoConfigureBF (Ptr<UnitFlow> flow, std::string extraInfo)
{
  // extract src ip, dst ip and device rate limit from info string
  std::stringstream ss(extraInfo);
  uint32_t srcIP;
  uint32_t dstIP;
  double deviceRateLimit;
  ss >> srcIP >> dstIP >> deviceRateLimit;

  // get IDs of the src host and dst host
  uint32_t src = -1;
  uint32_t dst = -1;
  for (auto it : m_hostList)
    {
      if (it->CheckIP (Ipv4Address (srcIP)))
        {
          src = it->GetHostId ();
        }
      if (it->CheckIP (Ipv4Address (dstIP)))
        {
          dst = it->GetHostId ();
        }
    }

  // compute the bandwidth function
  auto tenant = m_tenantTable[flow->GetTenantId ()];
  auto srcWeight = tenant->GetHostWeight (src);
  auto dstWeight = tenant->GetHostWeight (dst);
  Ptr<BandwidthFunction> newBF (new BandwidthFunction);
  newBF->AddVertex (deviceRateLimit / (srcWeight + dstWeight), deviceRateLimit);
  flow->SetConfiguredBF (newBF);
  NS_LOG_UNCOND ("Config TBF: " << *newBF << " for flow " << flow->GetTraceId ());

  // link the new flow to the tenant
  tenant->AddUnitFlow (flow);

  // actively transform all related bandwidth function
  tenant->TransformComponentialBF ();
}

double
BwmCoordinator::EstimateTargetStatus ()
{
  // implement the simple Target Status Estimation Algorithm
  double sum = 0;
  for (auto it : m_tenantTable)
    {
      sum += it.second->GetActualFS ();
    }

  return std::max((sum / m_tenantTable.size ()) * (1 + m_alpha), m_minFS); /*New FS*/
}

void
BwmCoordinator::SendNewArguments (Ptr<BwmLocalAgent> targetHost, double fairShare)
{
  targetHost->SetNewTargetStatus (fairShare);
}

void
BwmCoordinator::UpdateUsage (Ptr<BwmLocalAgent> host, std::list<Ptr<UnitFlow> > flowList)
{
  // update usage of all related tenants
  for (auto flow : flowList)
    {
      uint32_t tenantId = flow->GetTenantId ();
      m_tenantTable[tenantId]->UpdateUnitFlow (flow);
    }

  // compute new target status
  double newStatus = EstimateTargetStatus ();

  // send the new status to the host
  SendNewArguments(host, newStatus);
}

}
