
odinagent::OdinAgent(c0:4a:00:11:a6:ca, RT rates, CHANNEL 4, DEFAULT_GW 172.16.250.1, DEBUGFS /sys/kernel/debug/ieee80211/phy2/ath9k_htc/bssid_extra)
TimedSource(2, "ping
")->  odinsocket::Socket(UDP, 127.0.0.1, 2819, CLIENT true)



odinagent[3] -> odinsocket

rates :: AvailableRates(DEFAULT 24 36 48 108);

control :: ControlSocket("TCP", 6777);
chatter :: ChatterSocket("TCP", 6778);

// ----------------Packets going down
FromHost(ap, HEADROOM 50)
  -> fhcl :: Classifier(12/0806 20/0001, -)
  -> fh_arpr :: ARPResponder(127.0.0.1 3c:97:0e:9e:39:25) // Resolve STA's ARP
  -> ARPPrint("Resolving client's ARP by myself")
  -> ToHost(ap)



q :: Queue(200)
  -> SetTXRate (12)
  -> RadiotapEncap()
  -> to_dev :: ToDevice (mon0);

// Anything from host that isn't an ARP request
fhcl[1]
  -> [1]odinagent



// Not looking for an STA's ARP? Then let it pass.
fh_arpr[1]
  -> [1]odinagent

odinagent[2]
  -> q

// ----------------Packets going down


// ----------------Packets coming up
from_dev :: FromDevice(mon0, HEADROOM 50)
//  -> Print("Packet from mon0")
  -> RadiotapDecap()
  -> ExtraDecap()
  -> phyerr_filter :: FilterPhyErr()
  -> tx_filter :: FilterTX()
  -> dupe :: WifiDupeFilter()
  -> [0]odinagent

odinagent[0]
  -> q

// Data frames
odinagent[1]
  -> decap :: WifiDecap()
  -> RXStats
  -> arp_c :: Classifier(12/0806 20/0001, -)
  -> arp_resp::ARPResponder (172.16.250.1 00-1B-B3-67-6B-EE) // ARP fast path for STA
  -> [1]odinagent


// ARP Fast path fail. Re-write MAC address
// to reflect datapath or learning switch will drop it
arp_resp[1]
  -> ToHost(ap)


// Non ARP packets. Re-write MAC address to
// reflect datapath or learning switch will drop it
arp_c[1]
  -> ToHost(ap)
