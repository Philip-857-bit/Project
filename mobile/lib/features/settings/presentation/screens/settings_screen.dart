import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../../../../core/providers/auth_provider.dart';
import '../../../../core/providers/realtime_provider.dart';
import '../../../../core/services/mqtt_service.dart';

class SettingsScreen extends ConsumerWidget {
  const SettingsScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final realtimeState = ref.watch(realtimeProvider);

    return Scaffold(
      appBar: AppBar(title: const Text('Settings')),
      body: ListView(
        children: [
          // Connection Status
          _SettingsSection(
            title: 'Connection',
            children: [
              ListTile(
                leading: Icon(
                  realtimeState.isConnected ? Icons.cloud_done : Icons.cloud_off,
                  color: realtimeState.isConnected ? Colors.green : Colors.grey,
                ),
                title: const Text('Real-time Connection'),
                subtitle: Text(_getConnectionStatus(realtimeState.connectionState)),
                trailing: realtimeState.connectionState == AppMqttState.connecting
                    ? const SizedBox(
                        width: 20,
                        height: 20,
                        child: CircularProgressIndicator(strokeWidth: 2),
                      )
                    : Switch(
                        value: realtimeState.isConnected,
                        onChanged: (value) {
                          if (value) {
                            ref.read(realtimeProvider.notifier).connect();
                          } else {
                            ref.read(realtimeProvider.notifier).disconnect();
                          }
                        },
                      ),
              ),
              if (realtimeState.lastMessageAt != null)
                ListTile(
                  leading: const Icon(Icons.access_time),
                  title: const Text('Last Update'),
                  subtitle: Text(_formatLastUpdate(realtimeState.lastMessageAt!)),
                ),
            ],
          ),

          _SettingsSection(
            title: 'Account',
            children: [
              ListTile(
                leading: const Icon(Icons.person),
                title: const Text('Profile'),
                subtitle: const Text('Manage your account'),
                trailing: const Icon(Icons.arrow_forward_ios, size: 16),
                onTap: () {},
              ),
              ListTile(
                leading: const Icon(Icons.security),
                title: const Text('Security'),
                subtitle: const Text('Password, biometrics'),
                trailing: const Icon(Icons.arrow_forward_ios, size: 16),
                onTap: () {},
              ),
            ],
          ),

          _SettingsSection(
            title: 'Notifications',
            children: [
              SwitchListTile(
                secondary: const Icon(Icons.notifications),
                title: const Text('Push Notifications'),
                subtitle: const Text('Receive alerts and updates'),
                value: true,
                onChanged: (v) {},
              ),
              SwitchListTile(
                secondary: const Icon(Icons.warning),
                title: const Text('Alert Notifications'),
                subtitle: const Text('Low feed, temperature alerts'),
                value: true,
                onChanged: (v) {},
              ),
              SwitchListTile(
                secondary: const Icon(Icons.schedule),
                title: const Text('Feeding Reminders'),
                subtitle: const Text('Notify before scheduled feeds'),
                value: true,
                onChanged: (v) {},
              ),
            ],
          ),

          _SettingsSection(
            title: 'Units & Display',
            children: [
              ListTile(
                leading: const Icon(Icons.thermostat),
                title: const Text('Temperature Unit'),
                subtitle: const Text('Celsius'),
                trailing: const Icon(Icons.arrow_forward_ios, size: 16),
                onTap: () => _showUnitSelector(context, 'Temperature', ['Celsius', 'Fahrenheit']),
              ),
              ListTile(
                leading: const Icon(Icons.scale),
                title: const Text('Weight Unit'),
                subtitle: const Text('Grams'),
                trailing: const Icon(Icons.arrow_forward_ios, size: 16),
                onTap: () => _showUnitSelector(context, 'Weight', ['Grams', 'Ounces', 'Pounds']),
              ),
              ListTile(
                leading: const Icon(Icons.palette),
                title: const Text('Theme'),
                subtitle: const Text('System default'),
                trailing: const Icon(Icons.arrow_forward_ios, size: 16),
                onTap: () => _showThemeSelector(context),
              ),
            ],
          ),

          _SettingsSection(
            title: 'Data & Storage',
            children: [
              ListTile(
                leading: const Icon(Icons.storage),
                title: const Text('Clear Cache'),
                subtitle: const Text('Free up storage space'),
                onTap: () => _showClearCacheDialog(context),
              ),
              ListTile(
                leading: const Icon(Icons.download),
                title: const Text('Export Data'),
                subtitle: const Text('Download feeding history'),
                onTap: () {},
              ),
            ],
          ),

          _SettingsSection(
            title: 'About',
            children: [
              const ListTile(
                leading: Icon(Icons.info),
                title: Text('App Version'),
                subtitle: Text('1.0.0 (Build 1)'),
              ),
              ListTile(
                leading: const Icon(Icons.description),
                title: const Text('Terms of Service'),
                trailing: const Icon(Icons.open_in_new, size: 16),
                onTap: () {},
              ),
              ListTile(
                leading: const Icon(Icons.privacy_tip),
                title: const Text('Privacy Policy'),
                trailing: const Icon(Icons.open_in_new, size: 16),
                onTap: () {},
              ),
              ListTile(
                leading: const Icon(Icons.help),
                title: const Text('Help & Support'),
                trailing: const Icon(Icons.arrow_forward_ios, size: 16),
                onTap: () {},
              ),
            ],
          ),

          const SizedBox(height: 16),
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 16),
            child: OutlinedButton.icon(
              onPressed: () async {
                final confirm = await _showLogoutConfirmation(context);
                if (confirm == true) {
                  ref.read(realtimeProvider.notifier).disconnect();
                  await ref.read(authStateProvider.notifier).logout();
                  if (context.mounted) context.go('/login');
                }
              },
              icon: const Icon(Icons.logout, color: Colors.red),
              label: const Text('Sign Out', style: TextStyle(color: Colors.red)),
            ),
          ),
          const SizedBox(height: 32),
        ],
      ),
    );
  }

  String _getConnectionStatus(AppMqttState state) {
    switch (state) {
      case AppMqttState.connected:
        return 'Connected to server';
      case AppMqttState.connecting:
        return 'Connecting...';
      case AppMqttState.disconnected:
        return 'Disconnected';
      case AppMqttState.error:
        return 'Connection error';
    }
  }

  String _formatLastUpdate(DateTime time) {
    final diff = DateTime.now().difference(time);
    if (diff.inSeconds < 60) return '${diff.inSeconds} seconds ago';
    if (diff.inMinutes < 60) return '${diff.inMinutes} minutes ago';
    return '${diff.inHours} hours ago';
  }

  void _showUnitSelector(BuildContext context, String title, List<String> options) {
    showModalBottomSheet(
      context: context,
      builder: (ctx) => Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          Padding(
            padding: const EdgeInsets.all(16),
            child: Text(title, style: const TextStyle(fontWeight: FontWeight.bold, fontSize: 18)),
          ),
          ...options.map((option) => ListTile(
            title: Text(option),
            trailing: option == options.first ? const Icon(Icons.check, color: Colors.green) : null,
            onTap: () => Navigator.pop(ctx),
          )),
          const SizedBox(height: 16),
        ],
      ),
    );
  }

  void _showThemeSelector(BuildContext context) {
    showModalBottomSheet(
      context: context,
      builder: (ctx) => Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          const Padding(
            padding: EdgeInsets.all(16),
            child: Text('Theme', style: TextStyle(fontWeight: FontWeight.bold, fontSize: 18)),
          ),
          ListTile(
            leading: const Icon(Icons.brightness_auto),
            title: const Text('System default'),
            trailing: const Icon(Icons.check, color: Colors.green),
            onTap: () => Navigator.pop(ctx),
          ),
          ListTile(
            leading: const Icon(Icons.light_mode),
            title: const Text('Light'),
            onTap: () => Navigator.pop(ctx),
          ),
          ListTile(
            leading: const Icon(Icons.dark_mode),
            title: const Text('Dark'),
            onTap: () => Navigator.pop(ctx),
          ),
          const SizedBox(height: 16),
        ],
      ),
    );
  }

  void _showClearCacheDialog(BuildContext context) {
    showDialog(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Clear Cache'),
        content: const Text('This will clear cached data. Your account and settings will not be affected.'),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx), child: const Text('Cancel')),
          FilledButton(
            onPressed: () {
              Navigator.pop(ctx);
              ScaffoldMessenger.of(context).showSnackBar(
                const SnackBar(content: Text('Cache cleared')),
              );
            },
            child: const Text('Clear'),
          ),
        ],
      ),
    );
  }

  Future<bool?> _showLogoutConfirmation(BuildContext context) {
    return showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Sign Out'),
        content: const Text('Are you sure you want to sign out?'),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx, false), child: const Text('Cancel')),
          TextButton(
            onPressed: () => Navigator.pop(ctx, true),
            child: const Text('Sign Out', style: TextStyle(color: Colors.red)),
          ),
        ],
      ),
    );
  }
}

class _SettingsSection extends StatelessWidget {
  final String title;
  final List<Widget> children;

  const _SettingsSection({required this.title, required this.children});

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Padding(
          padding: const EdgeInsets.fromLTRB(16, 16, 16, 8),
          child: Text(
            title,
            style: Theme.of(context).textTheme.titleSmall?.copyWith(
              color: Theme.of(context).colorScheme.primary,
              fontWeight: FontWeight.bold,
            ),
          ),
        ),
        ...children,
        const Divider(),
      ],
    );
  }
}
