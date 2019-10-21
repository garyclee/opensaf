/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2010 The OpenSAF Foundation
 * Copyright Ericsson AB 2017 - All Rights Reserved.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. This file and program are licensed
 * under the GNU Lesser General Public License Version 2.1, February 1999.
 * The complete license can be accessed from the following location:
 * http://opensource.org/licenses/lgpl-license.php
 * See the Copying file included with the OpenSAF distribution for full
 * licensing terms.
 *
 * Author(s): GoAhead Software
 *            Ericsson AB
 *
 */

#include "dtm/dtmnd/multicast.h"
#include <arpa/inet.h>
#include <endian.h>
#include <inttypes.h>
#include <net/if.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <fstream>
#include "base/logtrace.h"
#include "base/osaf_socket.h"
#include "base/osaf_utility.h"

Multicast::Multicast(uint16_t cluster_id, uint32_t node_id,
                     in_port_t stream_port, in_port_t dgram_port,
                     sa_family_t address_family,
                     const std::string &stream_address,
                     const std::string &public_address,
                     const std::string &dgram_address,
                     const std::string &multicast_address,
                     const std::string &ifname, bool scope_link)
    : cluster_id_{cluster_id},
      node_id_{node_id},
      stream_port_{stream_port},
      dgram_port_{dgram_port},
      address_family_{address_family},
      stream_address_{stream_address},
      public_address_{public_address},
      dgram_address_{dgram_address},
      multicast_address_{multicast_address},
      ifname_{ifname},
      scope_link_{scope_link},
      protocol_{GetProtocol(multicast_address)},
      dgram_sock_sndr{-1},
      dgram_sock_rcvr{-1} {
  switch (protocol_) {
    case kBroadcast: {
      uint32_t rc = dtm_dgram_bcast_sender();
      if (rc == NCSCC_RC_SUCCESS) {
        rc = dtm_dgram_bcast_listener();
        if (rc != NCSCC_RC_SUCCESS) {
          LOG_ER("DTM:Set up the initial bcast  receiver socket failed");
        }
      } else {
        LOG_ER("DTM:Set up the initial bcast  sender socket failed rc: %d", rc);
      }
      break;
    }
    case kMulticast: {
      uint32_t rc = dtm_dgram_mcast_sender(64); /*TODO */
      if (rc == NCSCC_RC_SUCCESS) {
        rc = dtm_dgram_mcast_listener();
        if (rc != NCSCC_RC_SUCCESS) {
          LOG_ER("DTM:Set up the initial mcast  receiver socket failed");
        }
      } else {
        LOG_ER("DTM:Set up the initial mcast sender socket failed rc: %d", rc);
      }
      break;
    }
    case kFileUnicast: {
      GetPeersFromFile(multicast_address.substr(5));
      bool result = InitializeUnicastSender();
      if (result) {
        result = InitializeUnicastReceiver();
        if (!result) {
          LOG_ER("DTM:Set up the initial ucast  receiver socket failed");
        }
      } else {
        LOG_ER("DTM:Set up the initial ucast  sender socket failed");
      }
      break;
    }
    case kDnsUnicast: {
      GetPeersFromDns(multicast_address.substr(4));
      bool result = InitializeUnicastSender();
      if (result) {
        result = InitializeUnicastReceiver();
        if (!result) {
          LOG_ER("DTM:Set up the initial ucast  receiver socket failed");
        }
      } else {
        LOG_ER("DTM:Set up the initial ucast  sender socket failed");
      }
      break;
    }
  }

  if (peers_.empty()) {
    close(dgram_sock_rcvr);
    dgram_sock_rcvr = -1;
  }

  struct in_addr addr_ipv4;
  struct in6_addr addr_ipv6;
  int rc = -1;
  if (address_family_ == AF_INET) {
    rc = inet_pton(AF_INET, stream_address_.c_str(), &addr_ipv4);
  } else if (address_family_ == AF_INET6) {
    rc = inet_pton(AF_INET6, stream_address_.c_str(), &addr_ipv6);
  }
  unsigned ifindex = if_nametoindex(ifname_.c_str());
  if (rc == 1) {
    auto start = peers_.begin();
    auto end = peers_.end();
    while (start != end) {
      bool found = false;
      if (address_family_ == AF_INET &&
          start->address()->sa_family == AF_INET) {
        const struct sockaddr_in *peer =
            reinterpret_cast<const struct sockaddr_in *>(start->address());
        if (addr_ipv4.s_addr == peer->sin_addr.s_addr) found = true;
      } else if (address_family_ == AF_INET6 &&
                 start->address()->sa_family == AF_INET6) {
        const struct sockaddr_in6 *peer =
            reinterpret_cast<const struct sockaddr_in6 *>(start->address());
        if (IN6_ARE_ADDR_EQUAL(&addr_ipv6, &peer->sin6_addr) &&
            (!scope_link_ || ifindex == peer->sin6_scope_id))
          found = true;
      }
      auto iter = start;
      ++start;
      if (found) peers_.erase(iter);
    }
  }
  if (protocol_ == kFileUnicast || protocol_ == kDnsUnicast) {
    LOG_NO("Number of unicast peers: %zu", static_cast<size_t>(peers_.size()));
  }
}

Multicast::Protocol Multicast::GetProtocol(
    const std::string &multicast_address) {
  TRACE_ENTER();
  Multicast::Protocol p;
  if (multicast_address.empty()) {
    p = kBroadcast;
  } else if (multicast_address.compare(0, 5, "file:") == 0) {
    p = kFileUnicast;
  } else if (multicast_address.compare(0, 4, "dns:") == 0) {
    p = kDnsUnicast;
  } else {
    p = kMulticast;
  }
  TRACE_LEAVE2("Protocol = %d", static_cast<int>(p));
  return p;
}

bool Multicast::GetPeersFromFile(const std::string &path_name) {
  std::ifstream str;
  try {
    str.open(path_name);
    if (str.good()) {
      std::string cluster_name;
      std::getline(str, cluster_name);
      if (ValidateClusterName(cluster_name)) {
        LOG_NO("Cluster name: '%s' (from file '%s')", cluster_name.c_str(),
               path_name.c_str());
      } else {
        LOG_ER("Invalid cluster name '%s' (from file '%s')",
               cluster_name.c_str(), path_name.c_str());
        str.setstate(str.rdstate() | std::ios_base::failbit);
      }
    }
    while (str.good()) {
      std::string address;
      std::getline(str, address);
      if (!address.empty()) {
        struct sockaddr_storage addr;
        socklen_t len =
            ParseAddress(address_family_, address, dgram_port_, &addr);
        if (len != 0) {
          if (peers_.emplace(reinterpret_cast<sockaddr *>(&addr), len).second) {
            LOG_IN("Added IP '%s' from '%s'", address.c_str(),
                   path_name.c_str());
          } else {
            LOG_WA("Duplicate IP '%s' in '%s'", address.c_str(),
                   path_name.c_str());
          }
        } else {
          peers_.clear();
          str.setstate(str.rdstate() | std::ios_base::failbit);
        }
      }
    }
  } catch (std::ifstream::failure&) {
    LOG_ER("Caught std::ifstream::failure when reading file '%s', peers=%zu",
           path_name.c_str(), static_cast<size_t>(peers_.size()));
    peers_.clear();
  }
  if (peers_.empty()) {
    LOG_ER("Failed to read peers from file '%s'", path_name.c_str());
  }
  return !peers_.empty();
}

bool Multicast::ValidateClusterName(const std::string &cluster_name) {
  size_t no_of_colons = 0;
  size_t no_of_letters = 0;
  for (std::string::size_type i = 0; i != cluster_name.size(); ++i) {
    if (cluster_name[i] == ':') ++no_of_colons;
    if ((cluster_name[i] >= 'a' && cluster_name[i] <= 'z') ||
        (cluster_name[i] >= 'A' && cluster_name[i] <= 'Z')) {
      ++no_of_letters;
    }
  }
  return no_of_colons == 0 && no_of_letters != 0;
}

socklen_t Multicast::ParseAddress(sa_family_t address_family,
                                  const std::string &address, in_port_t port,
                                  struct sockaddr_storage *addr) {
  char local_port_str[8];
  snprintf(local_port_str, sizeof(local_port_str), "%" PRIu16, port);

  struct addrinfo addr_criteria {};
  addr_criteria.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;
  addr_criteria.ai_family = address_family;
  addr_criteria.ai_socktype = SOCK_DGRAM;
  addr_criteria.ai_protocol = IPPROTO_UDP;

  struct addrinfo *res;
  int rv = getaddrinfo(address.c_str(), local_port_str, &addr_criteria, &res);
  socklen_t addrlen;
  if (rv == 0) {
    addrlen = res->ai_addrlen;
    memcpy(addr, res->ai_addr, addrlen);
    freeaddrinfo(res);
  } else {
    LOG_ER("Invalid peer IP address '%s': %s", address.c_str(),
           gai_strerror(rv));
    addrlen = 0;
  }
  return addrlen;
}

bool Multicast::GetPeersFromDns(const std::string &domain_name) {
  char local_port_str[8];
  snprintf(local_port_str, sizeof(local_port_str), "%" PRIu16, dgram_port_);

  struct addrinfo addr_criteria {};
  addr_criteria.ai_flags = AI_IDN | AI_NUMERICSERV;
  addr_criteria.ai_family = address_family_;
  addr_criteria.ai_socktype = SOCK_DGRAM;
  addr_criteria.ai_protocol = IPPROTO_UDP;

  struct addrinfo *res;
  int rv =
      getaddrinfo(domain_name.c_str(), local_port_str, &addr_criteria, &res);
  if (rv == 0) {
    for (struct addrinfo *p = res; p != nullptr; p = p->ai_next) {
      char buf[INET6_ADDRSTRLEN];
      const char *result = nullptr;
      if (p->ai_family == AF_INET) {
        struct sockaddr_in *addr =
            reinterpret_cast<struct sockaddr_in *>(p->ai_addr);
        result = inet_ntop(p->ai_family, &addr->sin_addr, buf, sizeof(buf));
      } else if (p->ai_family == AF_INET6) {
        struct sockaddr_in6 *addr =
            reinterpret_cast<struct sockaddr_in6 *>(p->ai_addr);
        result = inet_ntop(p->ai_family, &addr->sin6_addr, buf, sizeof(buf));
      }
      if (peers_.emplace(p->ai_addr, p->ai_addrlen).second) {
        if (result != nullptr)
          LOG_IN("Added IP '%s' from '%s'", result, domain_name.c_str());
      } else {
        if (result != nullptr)
          LOG_WA("Duplicate IP '%s' from '%s'", result, domain_name.c_str());
      }
    }
    freeaddrinfo(res);
    if (!peers_.empty()) {
      LOG_NO("Cluster name: '%s' (from DNS)", domain_name.c_str());
    } else {
      LOG_ER("Failed to get any peers from domain name '%s'",
             domain_name.c_str());
    }
  } else {
    LOG_ER("getaddrinfo('%s', %s) failed: %s", domain_name.c_str(),
           local_port_str, gai_strerror(rv));
  }
  return !peers_.empty();
}

Multicast::~Multicast() {
  if (dgram_sock_sndr >= 0) close(dgram_sock_sndr);
  if (dgram_sock_rcvr >= 0) close(dgram_sock_rcvr);
}

bool Multicast::InitializeUnicastSender() {
  TRACE_ENTER();

  dgram_sock_sndr = socket(
      address_family_, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_UDP);
  if (dgram_sock_sndr < 0) {
    LOG_ER("socket() failed: %s", strerror(errno));
    TRACE_LEAVE();
    return false;
  }

  if (address_family_ == AF_INET6) {
    int v6only = 1;
    if (setsockopt(dgram_sock_sndr, IPPROTO_IPV6, IPV6_V6ONLY, &v6only,
                   sizeof(v6only)) < 0)
      LOG_ER("setsockopt(IPV6_V6ONLY) failed: %s", strerror(errno));
  }
  TRACE_LEAVE();
  return true;
}

bool Multicast::InitializeUnicastReceiver() {
  struct addrinfo *addr_list = nullptr, *p;  // Criteria for address
  int rv;
  char ucast_addr_eth[INET6_ADDRSTRLEN + IFNAMSIZ];
  TRACE_ENTER();

  TRACE("DTM :dgram_port_rcvr : %" PRIu16, dgram_port_);
  char local_port_str[8];
  snprintf(local_port_str, sizeof(local_port_str), "%" PRIu16, dgram_port_);

  dgram_sock_rcvr = -1;
  struct addrinfo addr_criteria {};
  addr_criteria.ai_flags = AI_PASSIVE | AI_NUMERICHOST | AI_NUMERICSERV;
  addr_criteria.ai_family = AF_UNSPEC;
  addr_criteria.ai_socktype = SOCK_DGRAM;
  addr_criteria.ai_protocol = IPPROTO_UDP;

  TRACE("DTM :ip_addr : %s local_port_str :%s", stream_address_.c_str(),
        local_port_str);
  if (address_family_ == AF_INET) {
    if ((rv = getaddrinfo(stream_address_.c_str(), local_port_str,
                          &addr_criteria, &addr_list)) != 0) {
      LOG_ER("DTM:Unable to getaddrinfo() for '%s': %s",
             stream_address_.c_str(), gai_strerror(rv));
      TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
      return NCSCC_RC_FAILURE;
    }
  } else if (address_family_ == AF_INET6) {
    if (scope_link_) {
      memset(ucast_addr_eth, 0, (INET6_ADDRSTRLEN + IFNAMSIZ));
      sprintf(ucast_addr_eth, "%s%%%s", stream_address_.c_str(),
              ifname_.c_str());
      if ((rv = getaddrinfo(ucast_addr_eth, local_port_str, &addr_criteria,
                            &addr_list)) != 0) {
        LOG_ER("DTM:Unable to getaddrinfo() for '%s': %s", ucast_addr_eth,
               gai_strerror(rv));
        TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
        return NCSCC_RC_FAILURE;
      }
    } else {
      if ((rv = getaddrinfo(stream_address_.c_str(), local_port_str,
                            &addr_criteria, &addr_list)) != 0) {
        LOG_ER("DTM:Unable to getaddrinfo() for '%s': %s",
               stream_address_.c_str(), gai_strerror(rv));
        TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
        return NCSCC_RC_FAILURE;
      }
    }
  }

  if (addr_list == nullptr) {
    LOG_ER("DTM:Unable to get addr_list ");
    TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
    return NCSCC_RC_FAILURE;
  }

  /* results and bind to the first we can */
  p = addr_list;
  bool bound = false;
  for (; p; p = p->ai_next) {
    TRACE("DTM :family : %d, socktype : %d, protocol :%d", p->ai_family,
          p->ai_socktype, p->ai_protocol);
    if (address_family_ != p->ai_family) {
      continue;
    }

    if (p->ai_family == AF_INET) {
      void *addr;
      char ipstr[INET6_ADDRSTRLEN];
      struct sockaddr_in *ipv4 =
          reinterpret_cast<struct sockaddr_in *>(p->ai_addr);
      addr = &(ipv4->sin_addr);
      inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr));
      if (strcasecmp(ipstr, stream_address_.c_str()) != 0) {
        continue;
      } else
        TRACE("DTM: DGRAM Socket bound to  = %s\n", ipstr);
    } else if (p->ai_family == AF_INET6) {
      void *addr;
      char ipstr[INET6_ADDRSTRLEN];
      struct sockaddr_in6 *ipv6 =
          reinterpret_cast<struct sockaddr_in6 *>(p->ai_addr);
      addr = &(ipv6->sin6_addr);
      inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr));
      if (strcasecmp(ipstr, stream_address_.c_str()) != 0) {
        continue;
      } else
        TRACE("DTM: DGRAM Socket bound to  = %s\n", ipstr);
    }
    if ((dgram_sock_rcvr =
             socket(p->ai_family, p->ai_socktype | SOCK_NONBLOCK | SOCK_CLOEXEC,
                    p->ai_protocol)) == -1) {
      LOG_ER("DTM:Socket creation failed (socket()) err :%s", strerror(errno));
      TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
      continue;
    }

    int smode = 1;
    if ((setsockopt(dgram_sock_rcvr, SOL_SOCKET, SO_REUSEADDR, &smode,
                    sizeof(smode)) == -1)) {
      LOG_ER("DTM : Error setsockpot: err :%s", strerror(errno));
      freeaddrinfo(addr_list);
      TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
      return NCSCC_RC_FAILURE;
    }
    if (p->ai_family == AF_INET6) {
      int v6only = 1;
      if (setsockopt(dgram_sock_rcvr, IPPROTO_IPV6, IPV6_V6ONLY, &v6only,
                     sizeof(v6only)) < 0)
        LOG_ER("setsockopt(IPV6_V6ONLY) failed: %s", strerror(errno));
    }
    if (osaf_bind(dgram_sock_rcvr, p->ai_addr, p->ai_addrlen) == -1) {
      LOG_ER("DTM:Socket bind failed  err :%s", strerror(errno));
      close(dgram_sock_rcvr);
      TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
      perror("listener: bind");
      freeaddrinfo(addr_list);
      return NCSCC_RC_FAILURE;
    } else {
      bound = true;
      break;
    }
  }

  /* Free address structure(s) allocated by getaddrinfo() */
  freeaddrinfo(addr_list);
  if (bound != true) {
    TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
    return NCSCC_RC_FAILURE;
  } else {
    TRACE_LEAVE2("rc :%d", NCSCC_RC_SUCCESS);
    return NCSCC_RC_SUCCESS;
  }
}

/**
 * Function for sending the bcast
 *
 * @param dtms_cb
 *
 * @return NCSCC_RC_SUCCESS
 * @return NCSCC_RC_FAILURE
 *
 */
uint32_t Multicast::dtm_dgram_bcast_sender() {
  TRACE_ENTER();

  dgram_sock_sndr = -1;
  socklen_t dest_addr_size = 0;
  struct sockaddr_storage dest_addr;

  if (address_family_ == AF_INET) {
    /* Holder for bcast_dest_address address */
    struct sockaddr_in *bcast_sender_addr_in =
        reinterpret_cast<struct sockaddr_in *>(&dest_addr);
    memset(bcast_sender_addr_in, 0, sizeof(struct sockaddr_in));
    bcast_sender_addr_in->sin_family = AF_INET;
    bcast_sender_addr_in->sin_port = htons(dgram_port_);
    TRACE("DTM:  IP address : %s  Bcast address : %s sa_family : %d ",
          stream_address_.c_str(), dgram_address_.c_str(), address_family_);
    int rc = inet_pton(AF_INET, dgram_address_.c_str(),
                       &bcast_sender_addr_in->sin_addr);
    if (rc != 1) {
      LOG_ER("DTM : inet_pton failed");
      TRACE_LEAVE2("rc :%d", rc);
      return NCSCC_RC_FAILURE;
    }

    memset(bcast_sender_addr_in->sin_zero, '\0',
           sizeof(bcast_sender_addr_in->sin_zero));
    dest_addr_size = sizeof(struct sockaddr_in);
  } else if (address_family_ == AF_INET6) {
    /* Holder for bcast_dest_address address */
    struct sockaddr_in6 *bcast_sender_addr_in6 =
        reinterpret_cast<struct sockaddr_in6 *>(&dest_addr);
    memset(bcast_sender_addr_in6, 0, sizeof(struct sockaddr_in6));
    bcast_sender_addr_in6->sin6_family = AF_INET6;
    bcast_sender_addr_in6->sin6_port = htons(dgram_port_);
    bcast_sender_addr_in6->sin6_flowinfo = 0;
    bcast_sender_addr_in6->sin6_scope_id = if_nametoindex(ifname_.c_str());
    TRACE("DTM:  IP address : %s  Bcast address : %s sa_family : %d ",
          stream_address_.c_str(), dgram_address_.c_str(), address_family_);
    inet_pton(AF_INET6, dgram_address_.c_str(),
              &bcast_sender_addr_in6->sin6_addr);
    dest_addr_size = sizeof(struct sockaddr_in6);
  } else {
    LOG_ER("DTM : dgram_enable_bcast failed");
    TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
    return NCSCC_RC_FAILURE;
  }
  if (peers_.emplace(reinterpret_cast<sockaddr *>(&dest_addr), dest_addr_size)
          .second) {
    LOG_IN("Added broadcast IP '%s'", dgram_address_.c_str());
  } else {
    osaf_abort(peers_.size());
  }

  /* Create socket for sending/receiving datagrams */
  dgram_sock_sndr =
      socket(dest_addr.ss_family, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
             IPPROTO_UDP);
  if (dgram_sock_sndr == -1) {
    LOG_ER("DTM :socket create  failederr :%s", strerror(errno));
    TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
    return NCSCC_RC_FAILURE;
  }

  if (dgram_enable_bcast(dgram_sock_sndr) != NCSCC_RC_SUCCESS) {
    LOG_ER("DTM : dgram_enable_bcast failed");
    if (close(dgram_sock_sndr) != 0 && errno != EINTR) {
      LOG_ER("Failed to close socket, errno=%d", errno);
    }
    TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
    return NCSCC_RC_FAILURE;
  }

  if (address_family_ == AF_INET6) {
    int v6only = 1;
    if (setsockopt(dgram_sock_sndr, IPPROTO_IPV6, IPV6_V6ONLY, &v6only,
                   sizeof(v6only)) < 0)
      LOG_ER("setsockopt(IPV6_V6ONLY) failed: %s", strerror(errno));
    struct sockaddr_in6 *bcast_sender_addr_in6 =
        reinterpret_cast<struct sockaddr_in6 *>(&dest_addr);
    int yes = 1;
    if (setsockopt(dgram_sock_sndr, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &yes,
                   sizeof(yes)) < 0) {
      LOG_ER("DTM :setsockopt(IPV6_MULTICAST_HOPS) failed err :%s",
             strerror(errno));
      TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
      return NCSCC_RC_FAILURE;
    }

    if (setsockopt(dgram_sock_sndr, SOL_SOCKET, SO_REUSEADDR, &yes,
                   sizeof(yes)) < 0) {
      LOG_ER("DTM :setsockopt(SO_REUSEADDR) failed err :%s", strerror(errno));
      TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
      return NCSCC_RC_FAILURE;
    }

    unsigned ifindex = if_nametoindex(ifname_.c_str());
    if (setsockopt(dgram_sock_sndr, IPPROTO_IPV6, IPV6_MULTICAST_IF, &ifindex,
                   sizeof(ifindex)) < 0) {
      LOG_ER("DTM :setsockopt(IPV6_MULTICAST_IF) failed err :%s ifname :%d",
             strerror(errno), if_nametoindex(ifname_.c_str()));
      TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
      return NCSCC_RC_FAILURE;
    }

    struct ipv6_mreq maddr;
    memset(&maddr, 0, sizeof(maddr));
    maddr.ipv6mr_multiaddr = bcast_sender_addr_in6->sin6_addr;
    maddr.ipv6mr_interface = if_nametoindex(ifname_.c_str());
    if (setsockopt(dgram_sock_sndr, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &maddr,
                   sizeof(maddr)) < 0) {
      LOG_ER("DTM :setsockopt(IPV6_ADD_MEMBERSHIP) failed err :%s",
             strerror(errno));
      TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
      return NCSCC_RC_FAILURE;
    }
  }
  TRACE_LEAVE2("rc :%d", NCSCC_RC_SUCCESS);
  return NCSCC_RC_SUCCESS;
}

/**
 * Enable the dgram bcast
 *
 * @param sock_desc
 *
 * @return NCSCC_RC_SUCCESS
 * @return NCSCC_RC_FAILURE
 *
 */
uint32_t Multicast::dgram_enable_bcast(int sock_desc) {
  TRACE_ENTER();
  /* If this fails, we'll hear about it when we try to send.  This will
   * allow */
  /* system that cannot bcast to continue if they don't plan to bcast */
  int bcast_permission = 1;
  if (setsockopt(sock_desc, SOL_SOCKET, SO_BROADCAST, &bcast_permission,
                 sizeof(bcast_permission)) < 0) {
    LOG_ER("DTM :setsockopt(SO_BROADCAST) failed err :%s ", strerror(errno));
    TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
    return NCSCC_RC_FAILURE;
  }

  TRACE_LEAVE2("rc :%d", NCSCC_RC_SUCCESS);
  return NCSCC_RC_SUCCESS;
}

/**
 * Function to send the mcast message
 *
 * @param dtms_cb mcast_ttl
 *  0 restricted to the same host
 *  1 restricted to the same subnet
 *  32 restricted to the same site
 *  64 restricted to the same region
 *  128 restricted to the same continent
 *  255 unrestricted
 *
 * @return NCSCC_RC_SUCCESS
 * @return NCSCC_RC_FAILURE
 *
 */
uint32_t Multicast::dtm_dgram_mcast_sender(int mcast_ttl) {
  /* Construct the serv address structure */
  int rv;
  TRACE_ENTER();

  dgram_sock_sndr = -1;

  TRACE("DTM :dgram_port_rcvr : %" PRIu16, dgram_port_);
  char local_port_str[8];
  snprintf(local_port_str, sizeof(local_port_str), "%" PRIu16, dgram_port_);

  struct addrinfo addr_criteria {};
  addr_criteria.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;
  addr_criteria.ai_family = AF_UNSPEC;
  addr_criteria.ai_socktype = SOCK_DGRAM;
  addr_criteria.ai_protocol = IPPROTO_UDP;

  struct addrinfo *mcast_sender_addr;
  char mcast_addr_eth[INET6_ADDRSTRLEN + IFNAMSIZ];
  memset(mcast_addr_eth, 0, (INET6_ADDRSTRLEN + IFNAMSIZ));
  /* For link-local address, need to set sin6_scope_id to match the
     device index of the network device on it has to connecct */
  if (scope_link_) {
    snprintf(mcast_addr_eth, sizeof(mcast_addr_eth), "%s%%%s",
             multicast_address_.c_str(), ifname_.c_str());
  } else {
    snprintf(mcast_addr_eth, sizeof(mcast_addr_eth), "%s",
             multicast_address_.c_str());
  }
  rv = getaddrinfo(mcast_addr_eth, local_port_str, &addr_criteria,
                   &mcast_sender_addr);
  TRACE("DTM :mcast_addr : %s local_port_str :%s", mcast_addr_eth,
        local_port_str);
  if (rv != 0) {
    LOG_ER("DTM:Unable to getaddrinfo(): %s", gai_strerror(rv));
    TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
    return NCSCC_RC_FAILURE;
  }

  if (mcast_sender_addr == nullptr) {
    LOG_ER("DTM:Unable to getaddrinfo() rtn_val :%d err :%s", rv,
           strerror(errno));
    TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
    return NCSCC_RC_FAILURE;
  }

  TRACE("DTM :family : %d, socktype : %d, protocol :%d",
        mcast_sender_addr->ai_family, mcast_sender_addr->ai_socktype,
        mcast_sender_addr->ai_protocol);
  /* Create socket for sending multicast datagrams */
  if ((dgram_sock_sndr =
           socket(mcast_sender_addr->ai_family,
                  mcast_sender_addr->ai_socktype | SOCK_NONBLOCK | SOCK_CLOEXEC,
                  mcast_sender_addr->ai_protocol)) == -1) {
    LOG_ER("DTM:Socket creation failed (socket()) err :%s", strerror(errno));
    freeaddrinfo(mcast_sender_addr);
    TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
    return NCSCC_RC_FAILURE;
  }

  if (dgram_set_mcast_ttl(mcast_ttl, mcast_sender_addr->ai_family) !=
      NCSCC_RC_SUCCESS) {
    LOG_ER("DTM : dgram_set_mcast_ttl() failed");
    if (close(dgram_sock_sndr) != 0 && errno != EINTR) {
      LOG_ER("Failed to close socket, errno=%d", errno);
    }
    freeaddrinfo(mcast_sender_addr);
    TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
    return NCSCC_RC_FAILURE;
  }

  if (peers_
          .emplace(reinterpret_cast<sockaddr *>(mcast_sender_addr->ai_addr),
                   mcast_sender_addr->ai_addrlen)
          .second) {
    LOG_IN("Added broadcast IP '%s'", mcast_addr_eth);
  } else {
    osaf_abort(peers_.size());
  }

  freeaddrinfo(mcast_sender_addr);

  TRACE_LEAVE2("rc :%d", NCSCC_RC_SUCCESS);
  return NCSCC_RC_SUCCESS;
}

Multicast::Message::Message(uint16_t i_cluster_id, uint32_t i_node_id,
                            bool i_use_multicast, in_port_t i_stream_port,
                            sa_family_t i_address_family,
                            const std::string &i_stream_address)
    : message_size{htobe16(sizeof(Message))},
      cluster_id{htobe16(i_cluster_id)},
      node_id{htobe32(i_node_id)},
      use_multicast{i_use_multicast ? uint8_t{1} : uint8_t{0}},
      stream_port{htobe16(i_stream_port)},
      address_family{static_cast<uint8_t>(i_address_family)},
      stream_address{} {
  size_t len = i_stream_address.size();
  if (len >= sizeof(stream_address) - 1) len = sizeof(stream_address) - 1;
  memcpy(stream_address, i_stream_address.data(), len);
  memset(stream_address + len, 0, sizeof(stream_address) - len);
}

/**
 * Set the mcast ttl
 *
 * @param dtms_cb mcast_ttl
 *
 * @return NCSCC_RC_SUCCESS
 * @return NCSCC_RC_FAILURE
 *
 */
uint32_t Multicast::dgram_set_mcast_ttl(int mcast_ttl, int family) {
  TRACE_ENTER();
  /* Set TTL of mcast packet. Unfortunately this requires */
  /* address-family-specific code */
  if (family == AF_INET6) {  // v6-specific
    int v6only = 1;
    if (setsockopt(dgram_sock_sndr, IPPROTO_IPV6, IPV6_V6ONLY, &v6only,
                   sizeof(v6only)) < 0)
      LOG_ER("setsockopt(IPV6_V6ONLY) failed: %s", strerror(errno));
    /* The v6 mcast TTL socket option requires that the value be */
    /* passed in as an integer */
    if (setsockopt(dgram_sock_sndr, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
                   &mcast_ttl, sizeof(mcast_ttl)) < 0) {
      LOG_ER("DTM : setsockopt(IPV6_MULTICAST_HOPS) failed err :%s",
             strerror(errno));
      TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
      return NCSCC_RC_FAILURE;
    }
    unsigned ifindex = if_nametoindex(ifname_.c_str());
    if (setsockopt(dgram_sock_sndr, IPPROTO_IPV6, IPV6_MULTICAST_IF, &ifindex,
                   sizeof(ifindex)) < 0) {
      LOG_ER("DTM :setsockopt(IPV6_MULTICAST_IF) failed err :%s ifname :%d",
             strerror(errno), if_nametoindex(ifname_.c_str()));
      TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
      return NCSCC_RC_FAILURE;
    }
  } else if (family == AF_INET) { /* v4 specific */
    if (setsockopt(dgram_sock_sndr, IPPROTO_IP, IP_MULTICAST_TTL, &mcast_ttl,
                   sizeof(mcast_ttl)) < 0) {
      LOG_ER("DTM :setsockopt(IP_MULTICAST_TTL) failed err :%s",
             strerror(errno));
      TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
      return NCSCC_RC_FAILURE;
    }
  } else {
    LOG_ER("DTM: AF not supported :%d", family);
    TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
    return NCSCC_RC_FAILURE;
  }

  TRACE_LEAVE2("rc :%d", NCSCC_RC_SUCCESS);
  return NCSCC_RC_SUCCESS;
}

bool Multicast::Send() {
  TRACE_ENTER();
  Message msg{cluster_id_,  node_id_,        !multicast_address_.empty(),
              stream_port_, address_family_,
              !public_address_.empty() ? public_address_ : stream_address_};
  bool success = true;
  for (const auto &addr : peers_) {
    ssize_t num_bytes;
    do {
      num_bytes = sendto(dgram_sock_sndr, &msg, sizeof(msg), 0, addr.address(),
                         addr.length());
    } while (num_bytes < 0 && errno == EINTR);
    if (num_bytes < 0) {
      LOG_ER("sendto() failed: %s ", strerror(errno));
      success = false;
    } else if (static_cast<size_t>(num_bytes) != sizeof(msg)) {
      LOG_ER("sendto() sent unexpected number of bytes %zd", num_bytes);
      success = false;
    }
  }

  TRACE_LEAVE2("success: %d", success ? 1 : 0);
  return success;
}

/**
 * Function to rcv the bcast message
 *
 * @param dtms_cb node_ip buffer buffer_len
 *
 * @return NCSCC_RC_SUCCESS
 * @return NCSCC_RC_FAILURE
 *
 */
ssize_t Multicast::Receive(void *buffer, size_t buffer_len) {
  TRACE_ENTER();

  ssize_t rtn;
  do {
    rtn = recv(dgram_sock_rcvr, buffer, buffer_len, MSG_DONTWAIT);
  } while (rtn < 0 && errno == EINTR);
  if (rtn < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
    LOG_ER("DTM:Receive failed (recv()) err :%s", strerror(errno));
  }

  TRACE_LEAVE2("rc :%zd", rtn);
  return rtn;
}

/**
 * Join the mcast group
 *
 * @param dtms_cb mcast_receiver_addr
 *
 * @return NCSCC_RC_SUCCESS
 * @return NCSCC_RC_FAILURE
 *
 */
uint32_t Multicast::dgram_join_mcast_group(
    struct addrinfo *mcast_receiver_addr) {
  TRACE_ENTER();

  /*  we need some address-family-specific pieces */
  if (mcast_receiver_addr->ai_family == AF_INET6) {
    /* Now join the mcast "group" (address) */
    struct ipv6_mreq join_request;
    memcpy(
        &join_request.ipv6mr_multiaddr,
        &(reinterpret_cast<struct sockaddr_in6 *>(mcast_receiver_addr->ai_addr))
             ->sin6_addr,
        sizeof(struct in6_addr));
    join_request.ipv6mr_interface = if_nametoindex(ifname_.c_str());
    TRACE("DTM :Joining IPv6 mcast group...");
    if (setsockopt(dgram_sock_rcvr, IPPROTO_IPV6, IPV6_JOIN_GROUP,
                   &join_request, sizeof(join_request)) < 0) {
      LOG_ER("DTM :setsockopt(IPV6_JOIN_GROUP) failed err :%s",
             strerror(errno));
      TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
      return NCSCC_RC_FAILURE;
    }
  } else if (mcast_receiver_addr->ai_family == AF_INET) {
    /* Now join the mcast "group" */
    struct ip_mreq join_request;
    memset(&join_request, 0, sizeof(join_request));
    join_request.imr_multiaddr =
        reinterpret_cast<struct sockaddr_in *>(mcast_receiver_addr->ai_addr)
            ->sin_addr;
    if (inet_aton(stream_address_.c_str(), &join_request.imr_interface) == 0) {
      LOG_ER("Invalid IPv4 address '%s'", stream_address_.c_str());
      TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
      return NCSCC_RC_FAILURE;
    }
    TRACE("DTM :Joining IPv4 mcast group...");
    if (setsockopt(dgram_sock_rcvr, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                   &join_request, sizeof(join_request)) < 0) {
      LOG_ER("DTM :setsockopt(IP_ADD_MEMBERSHIP) failed err :%s ",
             strerror(errno));
      TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
      return NCSCC_RC_FAILURE;
    }
  } else {
    LOG_ER("DTM: AF not supported :%d", mcast_receiver_addr->ai_family);
    TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
    return NCSCC_RC_FAILURE;
  }

  TRACE_LEAVE2("rc :%d", NCSCC_RC_SUCCESS);
  return NCSCC_RC_SUCCESS;
}

/**
 * Function to listen to mcast message
 *
 * @param dtms_cb
 *
 * @return NCSCC_RC_SUCCESS
 * @return NCSCC_RC_FAILURE
 *
 */
uint32_t Multicast::dtm_dgram_mcast_listener() {
  /* Construct the serv address structure */
  int rv;
  struct addrinfo *addr_list; /* Holder serv address */
  TRACE_ENTER();

  TRACE("DTM :dgram_port_rcvr : %" PRIu16, dgram_port_);
  char local_port_str[8];
  snprintf(local_port_str, sizeof(local_port_str), "%" PRIu16, dgram_port_);

  dgram_sock_rcvr = -1;

  struct addrinfo addr_criteria {};
  addr_criteria.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;
  addr_criteria.ai_family = AF_UNSPEC;
  addr_criteria.ai_socktype = SOCK_DGRAM;
  addr_criteria.ai_protocol = IPPROTO_UDP;

  /* For link-local address, need to set sin6_scope_id to match the
     device index of the network device on it has to connecct */
  if (scope_link_) {
    char mcast_addr_eth[INET6_ADDRSTRLEN + IFNAMSIZ];
    memset(mcast_addr_eth, 0, (INET6_ADDRSTRLEN + IFNAMSIZ));
    sprintf(mcast_addr_eth, "%s%%%s", multicast_address_.c_str(),
            ifname_.c_str());
    rv =
        getaddrinfo(mcast_addr_eth, local_port_str, &addr_criteria, &addr_list);
    TRACE("DTM:mcast_addr_eth : %s local_port_str :%s", mcast_addr_eth,
          local_port_str);
  } else {
    rv = getaddrinfo(multicast_address_.c_str(), local_port_str, &addr_criteria,
                     &addr_list);
    TRACE("DTM :mcast_addr : %s local_port_str :%s", multicast_address_.c_str(),
          local_port_str);
  }
  if (rv != 0) {
    LOG_ER("DTM:Unable to getaddrinfo(): %s", gai_strerror(rv));
    TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
    return NCSCC_RC_FAILURE;
  }

  if (addr_list == nullptr) {
    LOG_ER("DTM:Unable to getaddrinfo() rtn_val :%d err :%s", rv,
           strerror(errno));
    TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
    return NCSCC_RC_FAILURE;
  }

  TRACE("DTM :family : %d, socktype : %d, protocol :%d", addr_list->ai_family,
        addr_list->ai_socktype, addr_list->ai_protocol);
  /* Create socket for sending multicast datagrams */
  if ((dgram_sock_rcvr =
           socket(addr_list->ai_family,
                  addr_list->ai_socktype | SOCK_NONBLOCK | SOCK_CLOEXEC,
                  addr_list->ai_protocol)) == -1) {
    LOG_ER("DTM:Socket creation failed (socket()) err :%s", strerror(errno));
    freeaddrinfo(addr_list);
    TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
    return NCSCC_RC_FAILURE;
  }

  if (dgram_sock_rcvr == -1) {
    LOG_ER("DTM:Socket creation failed (socket())");
    freeaddrinfo(addr_list);
    TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
    return NCSCC_RC_FAILURE;
  }

  if (address_family_ == AF_INET6) {
    int v6only = 1;
    if (setsockopt(dgram_sock_rcvr, IPPROTO_IPV6, IPV6_V6ONLY, &v6only,
                   sizeof(v6only)) < 0)
      LOG_ER("setsockopt(IPV6_V6ONLY) failed: %s", strerror(errno));
  }

  if (osaf_bind(dgram_sock_rcvr, addr_list->ai_addr, addr_list->ai_addrlen) <
      0) {
    LOG_ER("DTM : osaf_bind() failed err :%s ", strerror(errno));
    if (close(dgram_sock_rcvr) != 0 && errno != EINTR)
      LOG_ER("close() failed, errno=%d", errno);
    freeaddrinfo(addr_list);
    TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
    return NCSCC_RC_FAILURE;
  }

  if (dgram_join_mcast_group(addr_list) != NCSCC_RC_SUCCESS) {
    LOG_ER("DTM : dgram_join_mcast_group() failed");
    if (close(dgram_sock_rcvr) != 0 && errno != EINTR)
      LOG_ER("close() failed, errno=%d", errno);
    freeaddrinfo(addr_list);
    TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
    return NCSCC_RC_FAILURE;
  }

  /* Free address structure(s) allocated by getaddrinfo() */
  freeaddrinfo(addr_list);
  TRACE_LEAVE2("rc :%d", NCSCC_RC_SUCCESS);
  return NCSCC_RC_SUCCESS;
}

/**
 * Function to listen to the bcast message
 *
 * @param dtms_cb
 *
 * @return NCSCC_RC_SUCCESS
 * @return NCSCC_RC_FAILURE
 *
 */
uint32_t Multicast::dtm_dgram_bcast_listener() {
  struct addrinfo *addr_list = nullptr, *p;  // Criteria for address
  int rv;
  char bcast_addr_eth[INET6_ADDRSTRLEN + IFNAMSIZ];
  TRACE_ENTER();

  TRACE("DTM :dgram_port_rcvr : %" PRIu16, dgram_port_);
  char local_port_str[8];
  snprintf(local_port_str, sizeof(local_port_str), "%" PRIu16, dgram_port_);

  dgram_sock_rcvr = -1;
  struct addrinfo addr_criteria {};
  addr_criteria.ai_flags = AI_PASSIVE | AI_NUMERICHOST | AI_NUMERICSERV;
  addr_criteria.ai_family = AF_UNSPEC;
  addr_criteria.ai_socktype = SOCK_DGRAM;
  addr_criteria.ai_protocol = IPPROTO_UDP;

  TRACE("DTM :ip_addr : %s local_port_str :%s", stream_address_.c_str(),
        local_port_str);
  if (address_family_ == AF_INET) {
    if ((rv = getaddrinfo(dgram_address_.c_str(), local_port_str,
                          &addr_criteria, &addr_list)) != 0) {
      LOG_ER("DTM:Unable to getaddrinfo() for '%s': %s", dgram_address_.c_str(),
             gai_strerror(rv));
      TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
      return NCSCC_RC_FAILURE;
    }
  } else if (address_family_ == AF_INET6) {
    if (scope_link_) {
      memset(bcast_addr_eth, 0, (INET6_ADDRSTRLEN + IFNAMSIZ));
      sprintf(bcast_addr_eth, "%s%%%s", dgram_address_.c_str(),
              ifname_.c_str());
      if ((rv = getaddrinfo(bcast_addr_eth, local_port_str, &addr_criteria,
                            &addr_list)) != 0) {
        LOG_ER("DTM:Unable to getaddrinfo() for '%s': %s", bcast_addr_eth,
               gai_strerror(rv));
        TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
        return NCSCC_RC_FAILURE;
      }
    } else {
      if ((rv = getaddrinfo(dgram_address_.c_str(), local_port_str,
                            &addr_criteria, &addr_list)) != 0) {
        LOG_ER("DTM:Unable to getaddrinfo() for '%s': %s",
               dgram_address_.c_str(), gai_strerror(rv));
        TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
        return NCSCC_RC_FAILURE;
      }
    }
  }

  if (addr_list == nullptr) {
    LOG_ER("DTM:Unable to get addr_list ");
    TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
    return NCSCC_RC_FAILURE;
  }

  /* results and bind to the first we can */
  p = addr_list;
  bool bound = false;

  for (; p; p = p->ai_next) {
    TRACE("DTM :family : %d, socktype : %d, protocol :%d", p->ai_family,
          p->ai_socktype, p->ai_protocol);
    if (address_family_ != p->ai_family) {
      continue;
    }

    if (p->ai_family == AF_INET) {
      void *addr;
      char ipstr[INET6_ADDRSTRLEN];
      struct sockaddr_in *ipv4 =
          reinterpret_cast<struct sockaddr_in *>(p->ai_addr);
      addr = &(ipv4->sin_addr);
      inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr));
      if (strcasecmp(ipstr, dgram_address_.c_str()) != 0) {
        continue;
      } else
        TRACE("DTM: DGRAM Socket bound to  = %s\n", ipstr);
    } else if (p->ai_family == AF_INET6) {
      void *addr;
      char ipstr[INET6_ADDRSTRLEN];
      struct sockaddr_in6 *ipv6 =
          reinterpret_cast<struct sockaddr_in6 *>(p->ai_addr);
      addr = &(ipv6->sin6_addr);
      inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr));
      if (strcasecmp(ipstr, dgram_address_.c_str()) != 0) {
        continue;
      } else
        TRACE("DTM: DGRAM Socket bound to  = %s\n", ipstr);
    }
    if ((dgram_sock_rcvr =
             socket(p->ai_family, p->ai_socktype | SOCK_NONBLOCK | SOCK_CLOEXEC,
                    p->ai_protocol)) == -1) {
      LOG_ER("DTM:Socket creation failed (socket()) err :%s", strerror(errno));
      TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
      continue;
    }

    int smode = 1;
    if ((setsockopt(dgram_sock_rcvr, SOL_SOCKET, SO_REUSEADDR, &smode,
                    sizeof(smode)) == -1)) {
      LOG_ER("DTM : Error setsockpot: err :%s", strerror(errno));
      freeaddrinfo(addr_list);
      TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
      return NCSCC_RC_FAILURE;
    }
    if (p->ai_family == AF_INET6) {
      int v6only = 1;
      if (setsockopt(dgram_sock_rcvr, IPPROTO_IPV6, IPV6_V6ONLY, &v6only,
                     sizeof(v6only)) < 0)
        LOG_ER("setsockopt(IPV6_V6ONLY) failed: %s", strerror(errno));
      struct sockaddr_in6 *ipv6 =
          reinterpret_cast<struct sockaddr_in6 *>(p->ai_addr);
      unsigned ifindex = if_nametoindex(ifname_.c_str());
      ipv6->sin6_scope_id = ifindex;
      if (setsockopt(dgram_sock_rcvr, IPPROTO_IPV6, IPV6_MULTICAST_IF, &ifindex,
                     sizeof(ifindex)) < 0) {
        LOG_ER("DTM :setsockopt(IPV6_MULTICAST_IF) failed err :%s",
               strerror(errno));
        freeaddrinfo(addr_list);
        TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
        return NCSCC_RC_FAILURE;
      }

      struct ipv6_mreq maddr;
      struct sockaddr_in6 *ipv6_mr =
          reinterpret_cast<struct sockaddr_in6 *>(p->ai_addr);
      memset(&maddr, 0, sizeof(maddr));
      maddr.ipv6mr_multiaddr = ipv6_mr->sin6_addr;
      maddr.ipv6mr_interface = if_nametoindex(ifname_.c_str());
      if (setsockopt(dgram_sock_rcvr, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &maddr,
                     sizeof(maddr)) < 0) {
        LOG_ER("DTM :setsockopt(IPV6_ADD_MEMBERSHIP) failed err :%s",
               strerror(errno));
        freeaddrinfo(addr_list);
        TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
        return NCSCC_RC_FAILURE;
      }
    }
    if (osaf_bind(dgram_sock_rcvr, p->ai_addr, p->ai_addrlen) == -1) {
      LOG_ER("DTM:Socket bind failed  err :%s", strerror(errno));
      close(dgram_sock_rcvr);
      TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
      perror("listener: bind");
      freeaddrinfo(addr_list);
      return NCSCC_RC_FAILURE;
    } else {
      bound = true;
      break;
    }
  }

  /* Free address structure(s) allocated by getaddrinfo() */
  freeaddrinfo(addr_list);
  if (bound != true) {
    TRACE_LEAVE2("rc :%d", NCSCC_RC_FAILURE);
    return NCSCC_RC_FAILURE;
  } else {
    TRACE_LEAVE2("rc :%d", NCSCC_RC_SUCCESS);
    return NCSCC_RC_SUCCESS;
  }
}
