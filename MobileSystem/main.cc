/*
Ebrar Begüm Şipal
Selcan Çelikel
 */

#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/core-module.h"
#include "ns3/forwarder-helper.h"
#include "ns3/gateway-lora-phy.h"
#include "ns3/hex-grid-position-allocator.h"
#include "ns3/log.h"
#include "ns3/lora-channel.h"
#include "ns3/lora-device-address-generator.h"
#include "ns3/lora-helper.h"
#include "ns3/lora-phy-helper.h"
#include "ns3/lorawan-mac-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/network-module.h"
#include "ns3/network-server-helper.h"
#include "ns3/periodic-sender-helper.h"
#include "ns3/periodic-sender.h"
#include "ns3/point-to-point-module.h"
#include "ns3/random-variable-stream.h"
#include "ns3/rectangle.h"
#include "ns3/string.h"
#include "ns3/ns2-mobility-helper.h"

using namespace ns3;
using namespace lorawan;

NS_LOG_COMPONENT_DEFINE("AdrExample");

/**
 * Record a change in the data rate setting on an end device.
 *
 * \param oldDr The previous data rate value.
 * \param newDr The updated data rate value.
 */
void
OnDataRateChange(uint8_t oldDr, uint8_t newDr)
{
    NS_LOG_DEBUG("DR" << unsigned(oldDr) << " -> DR" << unsigned(newDr));
}

/**
 * Record a change in the transmission power setting on an end device.
 *
 * \param oldTxPower The previous transmission power value.
 * \param newTxPower The updated transmission power value.
 */
void
OnTxPowerChange(double oldTxPower, double newTxPower)
{
    NS_LOG_DEBUG(oldTxPower << " dBm -> " << newTxPower << " dBm");
}

int nDevices = 0;
std::string outputFolder;

int
main(int argc, char* argv[])
{
    bool verbose = false;
    bool adrEnabled = true;
    bool initializeSF = false;
    int nPeriodsOf20Minutes =  10;
    double sideLengthMeters = 10000;
    int gatewayDistanceMeters = 5000;
    double maxRandomLossDB = 600;
    double minSpeedMetersPerSecond = 2;
    double maxSpeedMetersPerSecond = 16;
    std::string adrType = "ns3::AdrComponent";
    std::string tclFile = "filetcl.tcl";

    CommandLine cmd;


    cmd.AddValue("nDevices", "Number of end device nodes to create", nDevices);
    // cmd.AddValue("nGateways", "Number of gateway nodes to create", nGateways);
    cmd.AddValue("OutputFolder", "Output folder for results", outputFolder);
    cmd.Parse(argc, argv);


    int gatewayRings = 2 + (std::sqrt(2) * sideLengthMeters) / (gatewayDistanceMeters);
    int nGateways = 3 * gatewayRings * gatewayRings - 3 * gatewayRings + 1;

    // Logging
    //////////

    // LogComponentEnable("AdrExample", LOG_LEVEL_ALL);
    // // LogComponentEnable ("LoraPacketTracker", LOG_LEVEL_ALL);
    // // LogComponentEnable ("NetworkServer", LOG_LEVEL_ALL);
    // // LogComponentEnable ("NetworkController", LOG_LEVEL_ALL);
    // // LogComponentEnable ("NetworkScheduler", LOG_LEVEL_ALL);
    // // LogComponentEnable ("NetworkStatus", LOG_LEVEL_ALL);
    // // LogComponentEnable ("EndDeviceStatus", LOG_LEVEL_ALL);
    // LogComponentEnable("AdrComponent", LOG_LEVEL_ALL);
    // // LogComponentEnable("ClassAEndDeviceLorawanMac", LOG_LEVEL_ALL);
    // // LogComponentEnable ("LogicalLoraChannelHelper", LOG_LEVEL_ALL);
    // // LogComponentEnable ("MacCommand", LOG_LEVEL_ALL);
    // // LogComponentEnable ("AdrExploraSf", LOG_LEVEL_ALL);
    // // LogComponentEnable ("AdrExploraAt", LOG_LEVEL_ALL);
    // // LogComponentEnable ("EndDeviceLorawanMac", LOG_LEVEL_ALL);
    // LogComponentEnableAll(LOG_PREFIX_FUNC);
    // LogComponentEnableAll(LOG_PREFIX_NODE);
    // LogComponentEnableAll(LOG_PREFIX_TIME);

    // Set the end devices to allow data rate control (i.e. adaptive data rate) from the network
    // server
    Config::SetDefault("ns3::EndDeviceLorawanMac::DRControl", BooleanValue(true));

    // Create a simple wireless channel
    ///////////////////////////////////

    Ptr<LogDistancePropagationLossModel> loss = CreateObject<LogDistancePropagationLossModel>();
    loss->SetPathLossExponent(3.76);
    loss->SetReference(1, 7.7);

    Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable>();
    x->SetAttribute("Min", DoubleValue(0.0));
    x->SetAttribute("Max", DoubleValue(maxRandomLossDB));

    Ptr<RandomPropagationLossModel> randomLoss = CreateObject<RandomPropagationLossModel>();
    randomLoss->SetAttribute("Variable", PointerValue(x));

    loss->SetNext(randomLoss);

    Ptr<PropagationDelayModel> delay = CreateObject<ConstantSpeedPropagationDelayModel>();

    Ptr<LoraChannel> channel = CreateObject<LoraChannel>(loss, delay);

    // Helpers
    //////////

    // Gateway mobility
    MobilityHelper mobilityGw;
    Ptr<HexGridPositionAllocator> hexAllocator =
        CreateObject<HexGridPositionAllocator>(gatewayDistanceMeters / 2);
    mobilityGw.SetPositionAllocator(hexAllocator);
    mobilityGw.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    // Create the LoraPhyHelper
    LoraPhyHelper phyHelper = LoraPhyHelper();
    phyHelper.SetChannel(channel);

    // Create the LorawanMacHelper
    LorawanMacHelper macHelper = LorawanMacHelper();

    // Create the LoraHelper
    LoraHelper helper = LoraHelper();
    helper.EnablePacketTracking();

    ////////////////
    // Create gateways //
    ////////////////

    NodeContainer gateways;
    gateways.Create(nGateways);
    mobilityGw.Install(gateways);

    // Create the LoraNetDevices of the gateways
    phyHelper.SetDeviceType(LoraPhyHelper::GW);
    macHelper.SetDeviceType(LorawanMacHelper::GW);
    helper.Install(phyHelper, macHelper, gateways);

    // Create end devices
    /////////////

    NodeContainer endDevices;
    endDevices.Create(nDevices);

    // Install mobility model on mobile nodes using ns2 mobility trace
    Ns2MobilityHelper ns2Helper(tclFile);
    ns2Helper.Install();

    // Create a LoraDeviceAddressGenerator
    uint8_t nwkId = 54;
    uint32_t nwkAddr = 1864;
    Ptr<LoraDeviceAddressGenerator> addrGen =
        CreateObject<LoraDeviceAddressGenerator>(nwkId, nwkAddr);

    // Create the LoraNetDevices of the end devices
    phyHelper.SetDeviceType(LoraPhyHelper::ED);
    macHelper.SetDeviceType(LorawanMacHelper::ED_A);
    macHelper.SetAddressGenerator(addrGen);
    macHelper.SetRegion(LorawanMacHelper::EU);
    helper.Install(phyHelper, macHelper, endDevices);

    // Install applications in end devices
    int appPeriodSeconds = 200; // One packet every 20 minutes
    PeriodicSenderHelper appHelper = PeriodicSenderHelper();
    appHelper.SetPeriod(Seconds(appPeriodSeconds));
    ApplicationContainer appContainer = appHelper.Install(endDevices);

    // Do not set spreading factors up: we will wait for the network server to do this
    if (initializeSF)
    {
        LorawanMacHelper::SetSpreadingFactorsUp(endDevices, gateways, channel);
    }

    ////////////
    // Create network server
    ////////////

    Ptr<Node> networkServer = CreateObject<Node>();

    // PointToPoint links between gateways and server
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    // Store network server app registration details for later
    P2PGwRegistration_t gwRegistration;
    for (auto gw = gateways.Begin(); gw != gateways.End(); ++gw)
    {
        auto container = p2p.Install(networkServer, *gw);
        auto serverP2PNetDev = DynamicCast<PointToPointNetDevice>(container.Get(0));
        gwRegistration.emplace_back(serverP2PNetDev, *gw);
    }

    // Install the NetworkServer application on the network server
    NetworkServerHelper networkServerHelper;
    networkServerHelper.EnableAdr(adrEnabled);
    networkServerHelper.SetAdr(adrType);
    networkServerHelper.SetGatewaysP2P(gwRegistration);
    networkServerHelper.SetEndDevices(endDevices);
    networkServerHelper.Install(networkServer);

    // Install the Forwarder application on the gateways
    ForwarderHelper forwarderHelper;
    forwarderHelper.Install(gateways);

    // Connect our traces
    Config::ConnectWithoutContext(
        "/NodeList/*/DeviceList/0/$ns3::LoraNetDevice/Mac/$ns3::EndDeviceLorawanMac/TxPower",
        MakeCallback(&OnTxPowerChange));
    Config::ConnectWithoutContext(
        "/NodeList/*/DeviceList/0/$ns3::LoraNetDevice/Mac/$ns3::EndDeviceLorawanMac/DataRate",
        MakeCallback(&OnDataRateChange));

    // Activate printing of end device MAC parameters
    Time stateSamplePeriod = Seconds(400);
    helper.EnablePeriodicDeviceStatusPrinting(endDevices,
                                              gateways,
                                              "nodeData.txt",
                                              stateSamplePeriod);
    helper.EnablePeriodicPhyPerformancePrinting(gateways, "phyPerformance.txt", stateSamplePeriod);
    helper.EnablePeriodicGlobalPerformancePrinting("globalPerformance.txt", stateSamplePeriod);

    LoraPacketTracker& tracker = helper.GetPacketTracker();

    // Start simulation
    Time simulationTime = Seconds(400 * nPeriodsOf20Minutes);
    Simulator::Stop(simulationTime);
    Simulator::Run();
    Simulator::Destroy();

    std::cout << tracker.CountMacPacketsGlobally(Seconds(400 * (nPeriodsOf20Minutes - 2)),
                                                 Seconds(400 * (nPeriodsOf20Minutes - 1)))
              << std::endl;

    return 0;
}