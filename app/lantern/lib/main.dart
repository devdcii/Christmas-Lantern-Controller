import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:http/http.dart' as http;
import 'dart:convert';
import 'dart:async';
import 'dart:io';

void main() {
  runApp(const LanternControlApp());
}

class LanternControlApp extends StatelessWidget {
  const LanternControlApp({super.key});

  @override
  Widget build(BuildContext context) {
    return const CupertinoApp(
      title: 'Lantern Control',
      theme: CupertinoThemeData(
        brightness: Brightness.dark,
        primaryColor: CupertinoColors.systemBlue,
        scaffoldBackgroundColor: Color(0xFF1C1C1E),
      ),
      home: LanternControlScreen(),
      debugShowCheckedModeBanner: false,
    );
  }
}

class ESP32Service {
  static const String ESP32_IP = '192.168.4.1';
  String _baseUrl = 'http://$ESP32_IP';
  bool _isConnected = false;
  Timer? _connectionTimer;
  int _consecutiveFailures = 0;
  int _successfulConnections = 0;

  Function(bool)? onConnectionChanged;
  Function(String)? onStatusMessage;
  Function(Map<String, String>)? onDebugInfo;

  bool get isConnected => _isConnected;

  void startConnection() {
    _connectionTimer = Timer.periodic(Duration(seconds: 3), (timer) {
      _checkConnection();
    });
    _checkConnection();
  }

  Future<void> _checkConnection() async {
    try {
      var client = http.Client();
      final response = await client.get(
        Uri.parse('$_baseUrl/test'),
        headers: {'Content-Type': 'application/json'},
      ).timeout(Duration(seconds: 8));
      if (response.statusCode == 200) {
        final data = json.decode(response.body);
        if (data['status'] == 'success') {
          if (!_isConnected) {
            _isConnected = true;
            _consecutiveFailures = 0;
            _successfulConnections++;
            onConnectionChanged?.call(true);
            onStatusMessage?.call('Connected');
          }
          client.close();
          return;
        }
      }
      client.close();
      _handleConnectionFailure('HTTP ${response.statusCode}');
    } catch (e) {
      _handleConnectionFailure(e.toString());
    }
  }

  void _handleConnectionFailure(String error) {
    _consecutiveFailures++;
    if (_isConnected) {
      _isConnected = false;
      onConnectionChanged?.call(false);
      onStatusMessage?.call('Connection lost: $error');
    } else {
      onStatusMessage?.call('Connecting...');
    }
  }

  Future<Map<String, dynamic>?> getStatus() async {
    if (!_isConnected) return null;
    try {
      var client = http.Client();
      final response = await client.get(
        Uri.parse('$_baseUrl/api/status'),
        headers: {'Content-Type': 'application/json'},
      ).timeout(Duration(seconds: 8));
      client.close();
      if (response.statusCode == 200) {
        return json.decode(response.body);
      }
    } catch (_) {}
    return null;
  }

  Future<bool> sendCommand(String endpoint, Map<String, dynamic> data) async {
    if (!_isConnected) return false;
    try {
      var client = http.Client();
      final response = await client.post(
        Uri.parse('$_baseUrl/api/$endpoint'),
        headers: {'Content-Type': 'application/json'},
        body: json.encode(data),
      ).timeout(Duration(seconds: 8));
      client.close();
      return response.statusCode == 200;
    } catch (_) {
      _isConnected = false;
      onConnectionChanged?.call(false);
      onStatusMessage?.call('Command failed');
      return false;
    }
  }

  void dispose() {
    _connectionTimer?.cancel();
  }
}

class LightPattern {
  final String name;
  final int id;
  final IconData icon;
  final Color color;
  bool isActive;

  LightPattern({
    required this.name,
    required this.id,
    required this.icon,
    required this.color,
    this.isActive = false,
  });
}

class ColorOption {
  final String name;
  final Color color;
  final int r, g, b;
  bool isSelected;

  ColorOption({
    required this.name,
    required this.color,
    required this.r,
    required this.g,
    required this.b,
    this.isSelected = false,
  });
}

class CircleInfo {
  final int id;
  bool enabled;
  bool isSelected;
  int currentPattern;
  List<int> patternSequence;
  bool isSequenceMode;

  CircleInfo({
    required this.id,
    this.enabled = true,
    this.isSelected = false,
    this.currentPattern = 0,
    this.patternSequence = const [],
    this.isSequenceMode = false,
  });
}

class LanternControlScreen extends StatefulWidget {
  const LanternControlScreen({super.key});

  @override
  State<LanternControlScreen> createState() => _LanternControlScreenState();
}

class _LanternControlScreenState extends State<LanternControlScreen>
    with TickerProviderStateMixin {
  late ESP32Service _esp32Service;
  late AnimationController _breathingController;
  late AnimationController _rainbowController;

  bool _isConnected = false;
  String _statusMessage = 'Starting...';
  Timer? _statusTimer;
  Timer? _sequenceTimer;
  bool _isAllLightsOn = true;

  List<LightPattern> _patterns = [];
  List<ColorOption> _colors = [];
  List<CircleInfo> _circles = [];

  @override
  void initState() {
    super.initState();
    _breathingController = AnimationController(
      duration: Duration(seconds: 3),
      vsync: this,
    )..repeat(reverse: true);

    _rainbowController = AnimationController(
      duration: Duration(seconds: 5),
      vsync: this,
    )..repeat();

    _patterns = [
      LightPattern(name: 'Static', id: 0, icon: CupertinoIcons.lightbulb_fill, color: CupertinoColors.systemYellow),
      LightPattern(name: 'Breathing', id: 1, icon: CupertinoIcons.wind, color: CupertinoColors.systemTeal),
      LightPattern(name: 'Rainbow', id: 2, icon: CupertinoIcons.burst_fill, color: CupertinoColors.systemPurple),
      LightPattern(name: 'Chase', id: 3, icon: CupertinoIcons.arrow_right_circle, color: CupertinoColors.systemGreen),
      LightPattern(name: 'Twinkle', id: 4, icon: CupertinoIcons.sparkles, color: CupertinoColors.white),
      LightPattern(name: 'Wave', id: 5, icon: CupertinoIcons.waveform, color: CupertinoColors.systemCyan),
      LightPattern(name: 'Fire', id: 6, icon: CupertinoIcons.flame_fill, color: CupertinoColors.systemOrange),
      LightPattern(name: 'Christmas', id: 7, icon: CupertinoIcons.gift_fill, color: CupertinoColors.systemGreen),
      LightPattern(name: 'Circle Wave', id: 8, icon: CupertinoIcons.waveform, color: CupertinoColors.systemBlue),
      LightPattern(name: 'Alternating', id: 9, icon: CupertinoIcons.circle_grid_3x3_fill, color: CupertinoColors.systemPink),
    ];
    _colors = [
      ColorOption(name: 'Red', color: CupertinoColors.systemRed, r: 255, g: 0, b: 0, isSelected: true),
      ColorOption(name: 'Orange', color: CupertinoColors.systemOrange, r: 255, g: 149, b: 0),
      ColorOption(name: 'Yellow', color: CupertinoColors.systemYellow, r: 255, g: 204, b: 0),
      ColorOption(name: 'Green', color: CupertinoColors.systemGreen, r: 52, g: 199, b: 89),
      ColorOption(name: 'Blue', color: CupertinoColors.systemBlue, r: 0, g: 122, b: 255),
      ColorOption(name: 'Purple', color: CupertinoColors.systemPurple, r: 175, g: 82, b: 222),
      ColorOption(name: 'Pink', color: CupertinoColors.systemPink, r: 255, g: 45, b: 85),
      ColorOption(name: 'White', color: CupertinoColors.white, r: 255, g: 255, b: 255),
    ];
    _circles = List.generate(6, (index) => CircleInfo(id: index));
    _esp32Service = ESP32Service();

    _esp32Service.onConnectionChanged = (connected) {
      if (mounted) {
        setState(() {
          _isConnected = connected;
        });
        if (connected) {
          _startStatusUpdates();
          _startSequenceTimer();
        } else {
          _statusTimer?.cancel();
          _sequenceTimer?.cancel();
        }
      }
    };
    _esp32Service.onStatusMessage = (message) {
      if (mounted) {
        setState(() {
          _statusMessage = message;
        });
      }
    };
    _esp32Service.startConnection();
  }

  void _startStatusUpdates() {
    _statusTimer?.cancel();
    _statusTimer = Timer.periodic(Duration(seconds: 4), (timer) {
      _updateStatus();
    });
  }

  void _startSequenceTimer() {
    _sequenceTimer?.cancel();
    _sequenceTimer = Timer.periodic(Duration(seconds: 5), (timer) {
      _progressSequences();
    });
  }

  void _progressSequences() {
    for (int i = 0; i < _circles.length; i++) {
      final circle = _circles[i];
      if (circle.isSequenceMode && circle.patternSequence.isNotEmpty && circle.enabled) {
        int nextIndex = (circle.patternSequence.indexOf(circle.currentPattern) + 1) % circle.patternSequence.length;
        setState(() {
          _circles[i].currentPattern = circle.patternSequence[nextIndex];
        });
      }
    }
    _sendSequenceUpdates();
  }

  void _sendSequenceUpdates() async {
    for (final circle in _circles) {
      if (circle.isSequenceMode && circle.enabled) {
        await _esp32Service.sendCommand('circles', {
          'command': 'set_circle_pattern',
          'circle_id': circle.id,
          'pattern_id': circle.currentPattern,
        });
      }
    }
  }

  Future<void> _updateStatus() async {
    final status = await _esp32Service.getStatus();
    if (status != null && mounted) {
      setState(() {
        int currentPattern = status['pattern'] ?? 0;
        for (int i = 0; i < _patterns.length; i++) {
          _patterns[i].isActive = (i == currentPattern);
        }
        if (status['circles'] != null) {
          List circles = status['circles'];
          for (int i = 0; i < circles.length && i < _circles.length; i++) {
            if (!_circles[i].isSequenceMode) {
              _circles[i].enabled = circles[i]['enabled'] ?? true;
              _circles[i].currentPattern = currentPattern;
            }
          }
        }
        if (status['sequences'] != null) {
          Map<String, dynamic> sequences = status['sequences'];
          for (int i = 0; i < _circles.length; i++) {
            String circleKey = 'circle_$i';
            if (sequences.containsKey(circleKey)) {
              var seqData = sequences[circleKey];
              _circles[i].patternSequence = List<int>.from(seqData['patterns'] ?? []);
              _circles[i].isSequenceMode = _circles[i].patternSequence.isNotEmpty;
            }
          }
        }
      });
    }
  }

  @override
  void dispose() {
    _statusTimer?.cancel();
    _sequenceTimer?.cancel();
    _breathingController.dispose();
    _rainbowController.dispose();
    _esp32Service.dispose();
    super.dispose();
  }

  Future<void> _togglePattern(int index) async {
    final pattern = _patterns[index];
    bool hasSelectedCircles = _circles.any((c) => c.isSelected);

    if (hasSelectedCircles) {
      final selectedCircles = _circles.where((c) => c.isSelected).toList();
      for (var circle in selectedCircles) {
        await _esp32Service.sendCommand('circles', {
          'command': 'set_circle_pattern',
          'circle_id': circle.id,
          'pattern_id': pattern.id,
        });
      }
      setState(() {
        for (var circle in _circles) {
          circle.isSelected = false;
        }
        for (int i = 0; i < _patterns.length; i++) {
          _patterns[i].isActive = (i == index);
        }
        // Remove all pattern sequences and sequence mode for all circles
        for (var circle in _circles) {
          circle.patternSequence = [];
          circle.isSequenceMode = false;
          circle.currentPattern = pattern.id;
        }
      });
    } else {
      await _esp32Service.sendCommand('control', {'command': 'power_on'});
      await _esp32Service.sendCommand('control', {'command': 'set_pattern', 'value': pattern.id});
      if (_isConnected) {
        await _esp32Service.sendCommand('sequences', {
          'command': 'clear_all_sequences',
        });
      }
      setState(() {
        for (int i = 0; i < _patterns.length; i++) {
          _patterns[i].isActive = (i == index);
        }
        for (var circle in _circles) {
          circle.patternSequence = [];
          circle.isSequenceMode = false;
          circle.isSelected = false;
          circle.currentPattern = pattern.id;
        }
      });
    }
    HapticFeedback.lightImpact();
    _updateStatus();
  }

  Future<void> _selectColor(int index) async {
    setState(() {
      for (int i = 0; i < _colors.length; i++) {
        _colors[i].isSelected = (i == index);
      }
    });

    final selectedColor = _colors[index];
    await _esp32Service.sendCommand('color', {
      'r': selectedColor.r,
      'g': selectedColor.g,
      'b': selectedColor.b,
    });

    HapticFeedback.selectionClick();
  }

  void _toggleCircleSelection(int index) {
    setState(() {
      _circles[index].isSelected = !_circles[index].isSelected;
    });
    HapticFeedback.selectionClick();
  }

  Future<void> _setSequence(List<int> patternIds) async {
    final selectedCircles = _circles.where((c) => c.isSelected).toList();

    if (selectedCircles.isEmpty) {
      _showAlert('No Circles Selected', 'Please select circles first.');
      return;
    }

    setState(() {
      for (var circle in selectedCircles) {
        circle.patternSequence = patternIds;
        circle.isSequenceMode = patternIds.isNotEmpty;
        circle.currentPattern = patternIds.isNotEmpty ? patternIds[0] : 0;
        circle.isSelected = false; // Unselect after applying sequence
      }
    });

    if (_isConnected) {
      final circleIds = selectedCircles.map((c) => c.id).toList();
      await _esp32Service.sendCommand('sequences', {
        'command': 'set_sequence',
        'circles': circleIds,
        'patterns': patternIds,
      });
    }

    _showAlert('Sequence Set', 'Pattern sequence applied to selected circles');
  }

  Future<void> _clearSequence() async {
    final selectedCircles = _circles.where((c) => c.isSelected).toList();

    if (selectedCircles.isEmpty) {
      _showAlert('No Circles Selected', 'Please select circles first.');
      return;
    }

    setState(() {
      for (var circle in selectedCircles) {
        circle.patternSequence = [];
        circle.isSequenceMode = false;
        circle.currentPattern = 0;
        circle.isSelected = false;
      }
    });

    if (_isConnected) {
      final circleIds = selectedCircles.map((c) => c.id).toList();
      await _esp32Service.sendCommand('sequences', {
        'command': 'clear_sequence',
        'circles': circleIds,
      });
    }

    _showAlert('Sequence Cleared', 'Sequences cleared for selected circles');
  }

  void _showAlert(String title, String message) {
    showCupertinoDialog(
      context: context,
      builder: (context) => CupertinoAlertDialog(
        title: Text(title, style: TextStyle(color: CupertinoColors.white)),
        content: Text(message, style: TextStyle(color: CupertinoColors.white)),
        actions: [
          CupertinoDialogAction(
            child: Text('OK', style: TextStyle(color: CupertinoColors.white)),
            onPressed: () => Navigator.of(context).pop(),
          ),
        ],
      ),
    );
  }

  void _showAboutDialog() {
    showCupertinoDialog(
      context: context,
      builder: (context) => CupertinoAlertDialog(
        title: Text('About', style: TextStyle(color: CupertinoColors.white)),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            SizedBox(height: 16),
            Text('Developers:', style: TextStyle(color: CupertinoColors.white)),
            SizedBox(height: 12),
            Text('Cayabyab, Matt Julius M.', style: TextStyle(color: CupertinoColors.white, fontSize: 14)),
            Text('Deang, Ronnel O.', style: TextStyle(color: CupertinoColors.white, fontSize: 14)),
            Text('Digman, Christian D.', style: TextStyle(color: CupertinoColors.white, fontSize: 14)),
            Text('Paragas, John Ian Joseph M.', style: TextStyle(color: CupertinoColors.white, fontSize: 14)),
          ],
        ),
        actions: [
          CupertinoDialogAction(
            child: Text('OK', style: TextStyle(color: CupertinoColors.white)),
            onPressed: () => Navigator.of(context).pop(),
          ),
        ],
      ),
    );
  }

  void _showSequenceBuilder() {
    List<int> selectedPatterns = [];
    showCupertinoModalPopup<void>(
      context: context,
      builder: (BuildContext context) => StatefulBuilder(
        builder: (context, setModalState) {
          return Container(
            height: MediaQuery.of(context).size.height * 0.8,
            decoration: BoxDecoration(
              color: Color(0xFF2C2C2E),
              borderRadius: BorderRadius.only(
                topLeft: Radius.circular(20),
                topRight: Radius.circular(20),
              ),
            ),
            child: SafeArea(
              child: Column(
                children: [
                  Container(
                    padding: EdgeInsets.all(16),
                    child: Row(
                      children: [
                        Text('Build Pattern Sequence',
                            style: TextStyle(color: CupertinoColors.white, fontSize: 18)),
                        Spacer(),
                        GestureDetector(
                          onTap: () => Navigator.of(context).pop(),
                          child: Icon(CupertinoIcons.xmark_circle_fill, color: CupertinoColors.systemGrey),
                        ),
                      ],
                    ),
                  ),
                  Expanded(
                    child: Padding(
                      padding: EdgeInsets.all(16),
                      child: GridView.builder(
                        gridDelegate: SliverGridDelegateWithFixedCrossAxisCount(
                          crossAxisCount: 3,
                          childAspectRatio: 1.0,
                          crossAxisSpacing: 12,
                          mainAxisSpacing: 12,
                        ),
                        itemCount: _patterns.length,
                        itemBuilder: (context, index) {
                          final pattern = _patterns[index];
                          final isSelected = selectedPatterns.contains(index);
                          final selectionOrder = selectedPatterns.indexOf(index) + 1;
                          return GestureDetector(
                            onTap: () {
                              setModalState(() {
                                if (isSelected) {
                                  selectedPatterns.remove(index);
                                } else {
                                  selectedPatterns.add(index);
                                }
                              });
                            },
                            child: Container(
                              decoration: BoxDecoration(
                                color: isSelected
                                    ? CupertinoColors.systemBlue.withOpacity(0.3)
                                    : Color(0xFF3A3A3C),
                                borderRadius: BorderRadius.circular(12),
                                border: isSelected
                                    ? Border.all(color: CupertinoColors.systemBlue, width: 2)
                                    : null,
                              ),
                              child: Stack(
                                children: [
                                  Center(
                                    child: Column(
                                      mainAxisAlignment: MainAxisAlignment.center,
                                      children: [
                                        Icon(pattern.icon,
                                            size: 24,
                                            color: isSelected ? CupertinoColors.systemBlue : pattern.color),
                                        SizedBox(height: 8),
                                        Text(pattern.name,
                                          style: TextStyle(
                                            color: CupertinoColors.white,
                                            fontSize: 10,
                                          ),
                                          textAlign: TextAlign.center,
                                        ),
                                      ],
                                    ),
                                  ),
                                  if (isSelected)
                                    Positioned(
                                      top: 4,
                                      right: 4,
                                      child: Container(
                                        width: 20,
                                        height: 20,
                                        decoration: BoxDecoration(
                                          color: CupertinoColors.systemBlue,
                                          shape: BoxShape.circle,
                                        ),
                                        child: Center(
                                          child: Text('$selectionOrder',
                                            style: TextStyle(
                                              color: CupertinoColors.white,
                                              fontSize: 10,
                                            ),
                                          ),
                                        ),
                                      ),
                                    ),
                                ],
                              ),
                            ),
                          );
                        },
                      ),
                    ),
                  ),
                  Container(
                    padding: EdgeInsets.all(16),
                    child: Row(
                      children: [
                        Expanded(
                          child: CupertinoButton(
                            color: CupertinoColors.systemRed.withOpacity(0.8),
                            child: Text('Clear Sequence', style: TextStyle(color: CupertinoColors.white)),
                            onPressed: () {
                              _clearSequence();
                              Navigator.of(context).pop();
                            },
                          ),
                        ),
                        SizedBox(width: 12),
                        Expanded(
                          child: CupertinoButton(
                            color: selectedPatterns.isEmpty
                                ? CupertinoColors.systemGrey.withOpacity(0.3)
                                : CupertinoColors.systemBlue.withOpacity(0.8),
                            child: Text('Apply Sequence', style: TextStyle(color: CupertinoColors.white)),
                            onPressed: selectedPatterns.isEmpty ? null : () {
                              _setSequence(selectedPatterns);
                              Navigator.of(context).pop();
                            },
                          ),
                        ),
                      ],
                    ),
                  ),
                ],
              ),
            ),
          );
        },
      ),
    );
  }

  Widget _buildPatternIcon(int index) {
    final pattern = _patterns[index];
    if (index == 1 && pattern.isActive) {
      return AnimatedBuilder(
        animation: _breathingController,
        builder: (context, child) {
          return Icon(
            pattern.icon,
            size: 32,
            color: pattern.color.withOpacity(_breathingController.value * 0.5 + 0.5),
          );
        },
      );
    } else if (index == 2 && pattern.isActive) {
      final colors = [
        CupertinoColors.systemRed,
        CupertinoColors.systemOrange,
        CupertinoColors.systemYellow,
        CupertinoColors.systemGreen,
        CupertinoColors.systemBlue,
        CupertinoColors.systemPurple,
      ];
      final colorIndex = (_rainbowController.value * colors.length).floor() % colors.length;
      return Icon(pattern.icon, size: 32, color: colors[colorIndex]);
    }
    return Icon(
      pattern.icon,
      size: 32,
      color: pattern.isActive ? pattern.color : CupertinoColors.systemGrey,
    );
  }

  Future<void> _toggleAllLights() async {
    setState(() {
      _isAllLightsOn = !_isAllLightsOn;
    });

    if (_isAllLightsOn) {
      await _esp32Service.sendCommand('control', {'command': 'power_on'});
    } else {
      await _esp32Service.sendCommand('control', {'command': 'power_off'});
      setState(() {
        for (var pattern in _patterns) {
          pattern.isActive = false;
        }
      });
    }

    HapticFeedback.lightImpact();
  }

  @override
  Widget build(BuildContext context) {
    return CupertinoPageScaffold(
      backgroundColor: Color(0xFF1C1C1E),
      child: SafeArea(
        child: Column(
          children: [
            Container(
              width: double.infinity,
              padding: EdgeInsets.symmetric(horizontal: 16, vertical: 12),
              decoration: BoxDecoration(
                color: _isConnected
                    ? CupertinoColors.systemGreen.withOpacity(0.9)
                    : CupertinoColors.systemRed.withOpacity(0.9),
                borderRadius: BorderRadius.only(
                  bottomLeft: Radius.circular(8),
                  bottomRight: Radius.circular(8),
                ),
              ),
              child: Row(
                children: [
                  Icon(
                    _isConnected ? CupertinoIcons.wifi : CupertinoIcons.wifi_slash,
                    color: Colors.white,
                    size: 16,
                  ),
                  SizedBox(width: 8),
                  Expanded(
                    child: Text(
                      _isConnected ? 'CONNECTED' : 'CONNECTING...',
                      style: TextStyle(
                        color: Colors.white,
                        fontSize: 12,
                        fontWeight: FontWeight.w600,
                      ),
                    ),
                  ),
                  if (!_isConnected)
                    CupertinoActivityIndicator(color: Colors.white, radius: 8),
                  SizedBox(width: 8),
                  GestureDetector(
                    onTap: _showAboutDialog,
                    child: Icon(
                      CupertinoIcons.info_circle,
                      color: Colors.white,
                      size: 16,
                    ),
                  ),
                ],
              ),
            ),
            Container(
              padding: EdgeInsets.all(20),
              child: Row(
                children: [
                  Text(
                    'Christmas Lantern',
                    style: TextStyle(
                      color: CupertinoColors.white,
                      fontSize: 28,
                      fontWeight: FontWeight.w600,
                    ),
                  ),
                  Spacer(),
                  CupertinoButton(
                    padding: EdgeInsets.symmetric(horizontal: 16, vertical: 8),
                    color: _isAllLightsOn
                        ? CupertinoColors.systemGreen.withOpacity(0.8)
                        : CupertinoColors.systemRed.withOpacity(0.8),
                    child: Row(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        Icon(
                          _isAllLightsOn ? CupertinoIcons.lightbulb_fill : CupertinoIcons.lightbulb,
                          size: 16,
                          color: CupertinoColors.white,
                        ),
                        SizedBox(width: 8),
                        Text(
                          _isAllLightsOn ? 'ON' : 'OFF',
                          style: TextStyle(color: CupertinoColors.white),
                        ),
                      ],
                    ),
                    onPressed: _toggleAllLights,
                  ),
                ],
              ),
            ),
            SizedBox(height: 20),
            Container(
              padding: EdgeInsets.symmetric(horizontal: 20),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text('LED Color', style: TextStyle(color: CupertinoColors.white, fontSize: 16)),
                  SizedBox(height: 12),
                  SizedBox(
                    height: 50,
                    child: ListView.builder(
                      scrollDirection: Axis.horizontal,
                      itemCount: _colors.length,
                      itemBuilder: (context, index) {
                        final color = _colors[index];
                        return GestureDetector(
                          onTap: () => _selectColor(index),
                          child: Container(
                            margin: EdgeInsets.only(right: 12),
                            width: 50,
                            height: 50,
                            decoration: BoxDecoration(
                              color: color.color,
                              shape: BoxShape.circle,
                              border: color.isSelected
                                  ? Border.all(color: CupertinoColors.white, width: 3)
                                  : null,
                            ),
                            child: color.isSelected
                                ? Icon(CupertinoIcons.checkmark, color: Colors.black, size: 20)
                                : null,
                          ),
                        );
                      },
                    ),
                  ),
                ],
              ),
            ),
            SizedBox(height: 20),
            Container(
              padding: EdgeInsets.symmetric(horizontal: 20),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text('Circle Selection', style: TextStyle(color: CupertinoColors.white, fontSize: 16)),
                  SizedBox(height: 12),
                  SizedBox(
                    height: 60,
                    child: ListView.builder(
                      scrollDirection: Axis.horizontal,
                      itemCount: _circles.length,
                      itemBuilder: (context, index) {
                        final circle = _circles[index];
                        return GestureDetector(
                          onTap: () => _toggleCircleSelection(index),
                          child: Container(
                            margin: EdgeInsets.only(right: 12),
                            width: 60,
                            height: 60,
                            decoration: BoxDecoration(
                              color: circle.isSelected
                                  ? CupertinoColors.systemBlue.withOpacity(0.3)
                                  : Color(0xFF3A3A3C),
                              borderRadius: BorderRadius.circular(12),
                              border: circle.isSelected
                                  ? Border.all(color: CupertinoColors.systemBlue, width: 2)
                                  : null,
                            ),
                            child: Column(
                              mainAxisAlignment: MainAxisAlignment.center,
                              children: [
                                Text(
                                  '${circle.id + 1}',
                                  style: TextStyle(
                                    color: circle.isSelected
                                        ? CupertinoColors.systemBlue
                                        : CupertinoColors.white,
                                    fontSize: 18,
                                    fontWeight: FontWeight.bold,
                                  ),
                                ),
                              ],
                            ),
                          ),
                        );
                      },
                    ),
                  ),
                  SizedBox(height: 12),
                  CupertinoButton(
                    color: CupertinoColors.systemBlue.withOpacity(0.8),
                    child: Row(
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        Icon(CupertinoIcons.arrow_right_arrow_left_circle_fill, size: 16, color: CupertinoColors.white),
                        SizedBox(width: 8),
                        Text('Configure Sequence', style: TextStyle(color: CupertinoColors.white)),
                      ],
                    ),
                    onPressed: _showSequenceBuilder,
                  ),
                ],
              ),
            ),
            SizedBox(height: 20),
            Expanded(
              child: Container(
                padding: EdgeInsets.symmetric(horizontal: 20),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text('Light Patterns', style: TextStyle(color: CupertinoColors.white, fontSize: 16)),
                    SizedBox(height: 12),
                    Expanded(
                      child: GridView.builder(
                        gridDelegate: SliverGridDelegateWithFixedCrossAxisCount(
                          crossAxisCount: 3,
                          childAspectRatio: 0.9,
                          crossAxisSpacing: 16,
                          mainAxisSpacing: 16,
                        ),
                        itemCount: _patterns.length,
                        itemBuilder: (context, index) {
                          final pattern = _patterns[index];
                          return GestureDetector(
                            onTap: () => _togglePattern(index),
                            child: Container(
                              decoration: BoxDecoration(
                                color: pattern.isActive
                                    ? pattern.color.withOpacity(0.3)
                                    : Color(0xFF3A3A3C),
                                borderRadius: BorderRadius.circular(16),
                                border: pattern.isActive
                                    ? Border.all(color: pattern.color, width: 2)
                                    : null,
                              ),
                              child: Column(
                                mainAxisAlignment: MainAxisAlignment.center,
                                children: [
                                  _buildPatternIcon(index),
                                  SizedBox(height: 12),
                                  Text(
                                    pattern.name,
                                    style: TextStyle(
                                      color: CupertinoColors.white,
                                      fontSize: 12,
                                    ),
                                    textAlign: TextAlign.center,
                                  ),
                                ],
                              ),
                            ),
                          );
                        },
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}