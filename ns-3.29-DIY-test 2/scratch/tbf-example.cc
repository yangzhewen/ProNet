/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015 Universita' degli Studi di Napoli "Federico II"
 *               2017 Kungliga Tekniska Högskolan
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Pasquale Imputato <p.imputato@gmail.com>
 * Author: Stefano Avallone <stefano.avallone@unina.it>
 * Author: Surya Seetharaman <suryaseetharaman.9@gmail.com> - ported from ns-3
 *         RedQueueDisc traffic-control example to accommodate TbfQueueDisc example.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"

// This simple example shows how to use TrafficControlHelper to install a
// QueueDisc on a device.
//
// Network topology
//
//       10.1.1.0
// n0 -------------- n1
//    point-to-point
//
// The output will consist of all the traced changes in
// the number of tokens in TBF's first and second buckets:
//
//    FirstBucketTokens 0 to x
//    SecondBucketTokens 0 to x
//    FirstBucketTokens x to 0
//    SecondBucketTokens x to 0
//

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TbfExample");
std::ofstream rxOutput;
void
RxTrace (Ptr<const Packet> packet, const TcpHeader& header, Ptr<const TcpSocketBase> socket)
{
  FlowIdTag fidTag;
  packet->PeekPacketTag (fidTag);

  rxOutput << Simulator::Now ().GetSeconds () << ","
           <<"1,1" << ","
           << packet->GetSize () << std::endl;
}

void
RxSocketCreateTrace (Ptr<Socket> socket)
{
  socket->TraceConnectWithoutContext ("Rx", MakeCallback (RxTrace));
}


void
FirstBucketTokensTrace (uint32_t oldValue, uint32_t newValue)
{
  std::cout << "FirstBucketTokens " << oldValue << " to " << newValue << std::endl;
}

void
SecondBucketTokensTrace (uint32_t oldValue, uint32_t newValue)
{
  std::cout << "SecondBucketTokens " << oldValue << " to " << newValue << std::endl;
}

int
main (int argc, char *argv[])
{

  double simulationTime = 3; //seconds
  uint32_t burst = 10000;
  uint32_t mtu = 0;
  DataRate rate = DataRate ("1Gbps");
  DataRate peakRate = DataRate ("2Gbps");

  rxOutput.open ("rx-trace.txt");

  CommandLine cmd;
  cmd.AddValue ("burst", "Size of first bucket in bytes", burst);
  cmd.AddValue ("mtu", "Size of second bucket in bytes", mtu);
  cmd.AddValue ("rate", "Rate of tokens arriving in first bucket", rate);
  cmd.AddValue ("peakRate", "Rate of tokens arriving in second bucket", peakRate);

  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("1Gb/s"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("0ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  std::cout<<"rate="<<rate<<"\npeakrate="<<peakRate<<std::endl;

  TrafficControlHelper tch;
  uint16_t handle = tch.SetRootQueueDisc ("ns3::TbfQueueDisc",
                        "Burst", UintegerValue (burst),
                        "Mtu", UintegerValue (mtu),
                        "Rate", DataRateValue (DataRate (rate)),
                        "PeakRate", DataRateValue (DataRate (peakRate)));
  TrafficControlHelper::ClassIdList cid = tch.AddQueueDiscClasses (handle, 1, "ns3::QueueDiscClass");
  tch.AddChildQueueDisc (handle, cid[0], "ns3::WfqQueueDisc");
  QueueDiscContainer qdiscs = tch.Install (devices);



  Ptr<QueueDisc> q = qdiscs.Get (1);
//  q->TraceConnectWithoutContext ("TokensInFirstBucket", MakeCallback (&FirstBucketTokensTrace));
//  q->TraceConnectWithoutContext ("TokensInSecondBucket", MakeCallback (&SecondBucketTokensTrace));

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  //Flow
  uint16_t port = 7;
  Address localAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", localAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install (nodes.Get (0));

  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simulationTime + 0.1));

  sinkApp.Get (0)->TraceConnectWithoutContext ("SocketCreate", MakeCallback (RxSocketCreateTrace));

  uint32_t payloadSize = 1448;
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (payloadSize));

//  OnOffHelper onoff ("ns3::TcpSocketFactory", Ipv4Address::GetAny ());
//  onoff.SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
//  onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.2]"));
//  onoff.SetAttribute ("PacketSize", UintegerValue (payloadSize));
//  onoff.SetAttribute ("DataRate", StringValue ("1.1Mb/s")); //bit/s
//  ApplicationContainer apps;
//
//  InetSocketAddress rmt (interfaces.GetAddress (0), port);
//  rmt.SetTos (0xb8);
//  AddressValue remoteAddress (rmt);
//  onoff.SetAttribute ("Remote", remoteAddress);

  InetSocketAddress sinkAddr (nodes.Get (0)->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal (), port);
  ApplicationContainer sender;
  BulkSendHelper bulkHelper ("ns3::TcpSocketFactory", sinkAddr);
  sender = bulkHelper.Install (nodes.Get (1));
  sender.Start (Seconds (0.0));
  sender.Stop (Seconds (simulationTime));
  NS_LOG_INFO ("Flow " << " starts @ " << "0" << " ends @ " << simulationTime);
//  apps.Add (sender.Install (nodes.Get (1)));
//  apps.Start (Seconds (1.0));
//  apps.Stop (Seconds (simulationTime + 0.1));

  Simulator::Stop (Seconds (simulationTime + 5));
  Simulator::Run ();

  Simulator::Destroy ();

  std::cout << std::endl << "*** TC Layer statistics ***" << std::endl;
  std::cout << q->GetStats () << std::endl;
  return 0;
}
