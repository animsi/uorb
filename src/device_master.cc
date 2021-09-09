#include "device_master.h"

#include "device_node.h"

uorb::DeviceMaster uorb::DeviceMaster::instance_;

uorb::DeviceNode *uorb::DeviceMaster::CreateAdvertiser(
    const orb_metadata &meta, unsigned int *instance, unsigned int queue_size) {
  const bool is_single_instance = !instance;
  const unsigned max_group_tries =
      is_single_instance ? 1 : ORB_MULTI_MAX_INSTANCES;

  DeviceNode *device_node;
  unsigned group_tries = 0;

  uorb::base::WriterLockGuard lg(lock_);

  // Find the following devices that can advertise:
  // - Unadvertised device
  // - Single instance device
  // - Unregistered device
  do {
    device_node = GetDeviceNodeLocked(meta, group_tries);
    if (device_node &&
        (!device_node->publisher_count() || is_single_instance)) {
      device_node->set_queue_size(queue_size);
      device_node->add_publisher();
      break;  // Find a unadvertised device or single instance device
    }

    if (!device_node) {
      device_node = new DeviceNode(meta, group_tries, queue_size);
      if (!device_node) {
        errno = ENOMEM;
        return nullptr;
      }
      device_node->add_publisher();
      node_list_.Add(device_node);
      break;  // Create new device
    }
    group_tries++;
  } while (group_tries < max_group_tries);

  // All instances already exist
  if (group_tries >= max_group_tries) {
    errno = EEXIST;
    return nullptr;
  }

  if (instance) *instance = group_tries;
  return device_node;
}

uorb::DeviceNode *uorb::DeviceMaster::GetDeviceNode(const orb_metadata &meta,
                                                    uint8_t instance) const {
  uorb::base::ReaderLockGuard lg(lock_);
  return GetDeviceNodeLocked(meta, instance);
}

uorb::DeviceNode *uorb::DeviceMaster::GetDeviceNodeLocked(
    const orb_metadata &meta, uint8_t instance) const {
  // We can safely return the node that can be used by any thread, because a
  // DeviceNode never gets deleted.
  for (auto node : node_list_) {
    if (node->IsSameWith(meta, instance)) return node;
  }

  return nullptr;
}

uorb::DeviceNode *uorb::DeviceMaster::OpenDeviceNode(const orb_metadata &meta,
                                                     unsigned int instance) {
  if (instance >= ORB_MULTI_MAX_INSTANCES) {
    return nullptr;
  }

  uorb::base::WriterLockGuard lg(lock_);

  DeviceNode *device_node = GetDeviceNodeLocked(meta, instance);
  if (device_node) {
    return device_node;
  }

  device_node = new DeviceNode(meta, instance);

  if (!device_node) {
    errno = ENOMEM;
    return nullptr;
  }

  node_list_.Add(device_node);

  return device_node;  // Create new device
}
