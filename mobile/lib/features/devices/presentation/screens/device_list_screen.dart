import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../../../../core/config/env_config.dart';
import '../../../../core/theme/app_theme.dart';
import '../../../../core/providers/device_provider.dart';
import '../../../../core/models/device.dart';

class DeviceListScreen extends ConsumerStatefulWidget {
  const DeviceListScreen({super.key});

  @override
  ConsumerState<DeviceListScreen> createState() => _DeviceListScreenState();
}

class _DeviceListScreenState extends ConsumerState<DeviceListScreen> {
  @override
  void initState() {
    super.initState();
    ref.read(deviceListProvider.notifier).loadDevices();
  }

  @override
  Widget build(BuildContext context) {
    final deviceState = ref.watch(deviceListProvider);
    final showDebug = EnvConfig.isDevelopment || EnvConfig.debugMode;

    return Scaffold(
      appBar: AppBar(
        title: const Text('My Devices'),
        actions: [
          IconButton(
            icon: const Icon(Icons.add),
            onPressed: () => context.go('/devices/pair'),
          ),
        ],
      ),
      body: RefreshIndicator(
        onRefresh: () => ref.read(deviceListProvider.notifier).loadDevices(),
        child:
            deviceState.isLoading
                ? Center(
                  child: Column(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      const CircularProgressIndicator(),
                      if (showDebug) ...[
                        const SizedBox(height: 16),
                        _DebugPanel(
                          lines: [
                            'API: ${EnvConfig.apiBaseUrl}',
                            'Loading: ${deviceState.isLoading}',
                            'Count: ${deviceState.devices.length}',
                            'Error: ${deviceState.error ?? '-'}',
                            'Last request: ${_formatDebugTime(deviceState.lastRequestAt)}',
                            'Last success: ${_formatDebugTime(deviceState.lastSuccessAt)}',
                            'Status: ${deviceState.lastStatusCode?.toString() ?? '-'}',
                          ],
                        ),
                      ],
                    ],
                  ),
                )
                : deviceState.error != null
                ? Center(
                  child: Column(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: [
                      Icon(
                        Icons.error_outline,
                        size: 48,
                        color: Colors.grey[400],
                      ),
                      const SizedBox(height: 16),
                      Text(
                        deviceState.error!,
                        style: TextStyle(color: Colors.grey[600]),
                      ),
                      const SizedBox(height: 16),
                        ElevatedButton(
                          onPressed:
                              () =>
                                  ref
                                      .read(deviceListProvider.notifier)
                                      .loadDevices(),
                          child: const Text('Retry'),
                        ),
                        if (showDebug) ...[
                          const SizedBox(height: 16),
                          _DebugPanel(
                            lines: [
                              'API: ${EnvConfig.apiBaseUrl}',
                              'Loading: ${deviceState.isLoading}',
                              'Count: ${deviceState.devices.length}',
                              'Error: ${deviceState.error ?? '-'}',
                              'Last request: ${_formatDebugTime(deviceState.lastRequestAt)}',
                              'Last success: ${_formatDebugTime(deviceState.lastSuccessAt)}',
                              'Status: ${deviceState.lastStatusCode?.toString() ?? '-'}',
                            ],
                          ),
                        ],
                    ],
                  ),
                )
                : deviceState.devices.isEmpty
                ? _buildEmptyState(context)
                : ListView.separated(
                  padding: const EdgeInsets.all(16),
                  itemCount: deviceState.devices.length,
                  separatorBuilder: (_, _) => const SizedBox(height: 12),
                  itemBuilder: (context, index) {
                    final device = deviceState.devices[index];
                    return _DeviceCard(
                      device: device,
                      onTap: () => context.go('/devices/${device.id}'),
                    );
                  },
                ),
      ),
      floatingActionButton: FloatingActionButton.extended(
        onPressed: () => context.go('/devices/pair'),
        icon: const Icon(Icons.add),
        label: const Text('Add Device'),
      ),
    );
  }

  String _formatDebugTime(DateTime? time) {
    if (time == null) return '-';
    return '${time.hour.toString().padLeft(2, '0')}:'
        '${time.minute.toString().padLeft(2, '0')}:'
        '${time.second.toString().padLeft(2, '0')}';
  }

  Widget _buildEmptyState(BuildContext context) {
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(32),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(Icons.devices, size: 64, color: Colors.grey[400]),
            const SizedBox(height: 16),
            Text(
              'No devices yet',
              style: Theme.of(
                context,
              ).textTheme.titleLarge?.copyWith(color: Colors.grey[600]),
            ),
            const SizedBox(height: 8),
            Text(
              'Add your first fish feeder to get started',
              style: TextStyle(color: Colors.grey[500]),
              textAlign: TextAlign.center,
            ),
            const SizedBox(height: 24),
            ElevatedButton.icon(
              onPressed: () => context.go('/devices/pair'),
              icon: const Icon(Icons.add),
              label: const Text('Add Device'),
            ),
          ],
        ),
      ),
    );
  }
}

class _DebugPanel extends StatelessWidget {
  final List<String> lines;

  const _DebugPanel({required this.lines});

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Card(
      color: theme.colorScheme.surfaceContainerHighest,
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              'Devices debug',
              style: theme.textTheme.titleSmall?.copyWith(
                fontWeight: FontWeight.bold,
              ),
            ),
            const SizedBox(height: 6),
            ...lines.map(
              (line) => Padding(
                padding: const EdgeInsets.only(bottom: 2),
                child: Text(
                  line,
                  style: theme.textTheme.bodySmall,
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _DeviceCard extends StatelessWidget {
  final Device device;
  final VoidCallback onTap;

  const _DeviceCard({required this.device, required this.onTap});

  @override
  Widget build(BuildContext context) {
    return Card(
      child: InkWell(
        onTap: onTap,
        borderRadius: BorderRadius.circular(12),
        child: Padding(
          padding: const EdgeInsets.all(16),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Row(
                children: [
                  Icon(
                    Icons.router,
                    color:
                        device.isOnline
                            ? AppTheme.deviceOnline
                            : AppTheme.deviceOffline,
                    size: 32,
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          device.name,
                          style: Theme.of(context).textTheme.titleMedium
                              ?.copyWith(fontWeight: FontWeight.bold),
                        ),
                        Text(
                          device.serialNumber,
                          style: Theme.of(
                            context,
                          ).textTheme.bodySmall?.copyWith(color: Colors.grey),
                        ),
                      ],
                    ),
                  ),
                  Container(
                    padding: const EdgeInsets.symmetric(
                      horizontal: 8,
                      vertical: 4,
                    ),
                    decoration: BoxDecoration(
                      color:
                          device.isOnline
                              ? AppTheme.deviceOnline.withValues(alpha: 0.2)
                              : AppTheme.deviceOffline.withValues(alpha: 0.2),
                      borderRadius: BorderRadius.circular(12),
                    ),
                    child: Text(
                      device.isOnline ? 'Online' : 'Offline',
                      style: TextStyle(
                        color:
                            device.isOnline
                                ? AppTheme.deviceOnline
                                : AppTheme.deviceOffline,
                        fontSize: 12,
                        fontWeight: FontWeight.w500,
                      ),
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 16),
              Row(
                children: [
                  Expanded(
                    child: _StatusIndicator(
                      icon: Icons.battery_charging_full,
                      label: 'Battery',
                      value: '${device.status.batteryLevel.toInt()}%',
                      color: _getBatteryColor(device.status.batteryLevel),
                    ),
                  ),
                  Expanded(
                    child: _StatusIndicator(
                      icon: Icons.inventory_2,
                      label: 'Feed Level',
                      value: '${device.status.feedLevel.toInt()}%',
                      color: _getFeedLevelColor(device.status.feedLevel),
                    ),
                  ),
                  Expanded(
                    child: _StatusIndicator(
                      icon: Icons.access_time,
                      label: 'Last Seen',
                      value: _formatLastSeen(device.lastSeen),
                      color: Colors.grey,
                    ),
                  ),
                ],
              ),
            ],
          ),
        ),
      ),
    );
  }

  String _formatLastSeen(DateTime? lastSeen) {
    if (lastSeen == null) return 'Never';
    final diff = DateTime.now().difference(lastSeen);
    if (diff.inMinutes < 1) return 'Just now';
    if (diff.inMinutes < 60) return '${diff.inMinutes}m ago';
    if (diff.inHours < 24) return '${diff.inHours}h ago';
    return '${diff.inDays}d ago';
  }

  Color _getBatteryColor(double level) {
    if (level > 50) return AppTheme.batteryFull;
    if (level > 20) return AppTheme.batteryMedium;
    return AppTheme.batteryLow;
  }

  Color _getFeedLevelColor(double level) {
    if (level > 50) return AppTheme.feedLevelHigh;
    if (level > 20) return AppTheme.feedLevelMedium;
    return AppTheme.feedLevelLow;
  }
}

class _StatusIndicator extends StatelessWidget {
  final IconData icon;
  final String label;
  final String value;
  final Color color;

  const _StatusIndicator({
    required this.icon,
    required this.label,
    required this.value,
    required this.color,
  });

  @override
  Widget build(BuildContext context) {
    return Column(
      children: [
        Icon(icon, color: color, size: 20),
        const SizedBox(height: 4),
        Text(
          value,
          style: Theme.of(
            context,
          ).textTheme.bodyMedium?.copyWith(fontWeight: FontWeight.bold),
        ),
        Text(
          label,
          style: Theme.of(
            context,
          ).textTheme.bodySmall?.copyWith(color: Colors.grey),
        ),
      ],
    );
  }
}
