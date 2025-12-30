// Package mqtt provides MQTT message handlers for processing device messages
package mqtt

import (
	"context"
	"encoding/json"
	"log"
	"sync"

	"smart-fish-feeder/internal/mqtt/protobuf"
)

// DeviceMessageHandler defines the interface for handling device MQTT messages
type DeviceMessageHandler interface {
	HandleTelemetry(ctx context.Context, deviceID string, telemetry *protobuf.DeviceTelemetry) error
	HandleFeedingEvent(ctx context.Context, deviceID string, event *protobuf.FeedingEvent) error
	HandleAlert(ctx context.Context, deviceID string, alert *protobuf.DeviceAlert) error
	HandleCommandResponse(ctx context.Context, deviceID string, response *protobuf.CommandResponse) error
	HandleDiagnostics(ctx context.Context, deviceID string, report *protobuf.DiagnosticsReport) error
	HandleVisionAnalysis(ctx context.Context, deviceID string, analysis *protobuf.VisionAnalysis) error
}

// MQTTHandlers manages MQTT message handling
type MQTTHandlers struct {
	client         *Client
	shadowService  *DeviceShadowService
	messageHandler DeviceMessageHandler
	subscriptions  map[string]bool
	mu             sync.RWMutex
}

// NewMQTTHandlers creates a new MQTTHandlers instance
func NewMQTTHandlers(client *Client, shadowService *DeviceShadowService, handler DeviceMessageHandler) *MQTTHandlers {
	return &MQTTHandlers{
		client:         client,
		shadowService:  shadowService,
		messageHandler: handler,
		subscriptions:  make(map[string]bool),
	}
}

// SubscribeToDevice subscribes to all topics for a specific device
func (h *MQTTHandlers) SubscribeToDevice(deviceID string) error {
	h.mu.Lock()
	defer h.mu.Unlock()

	tb := NewTopicBuilder(deviceID)
	topics := []string{
		tb.Telemetry(),
		tb.FeedingEvent(),
		tb.Alert(),
		tb.Status(),
		tb.ShadowUpdate(),
	}

	for _, topic := range topics {
		if h.subscriptions[topic] {
			continue
		}

		callback := h.createMessageCallback(topic)
		if err := h.client.Subscribe(topic, 1, callback); err != nil {
			return err
		}
		h.subscriptions[topic] = true
	}

	return nil
}

// UnsubscribeFromDevice unsubscribes from all topics for a specific device
func (h *MQTTHandlers) UnsubscribeFromDevice(deviceID string) error {
	h.mu.Lock()
	defer h.mu.Unlock()

	tb := NewTopicBuilder(deviceID)
	topics := []string{
		tb.Telemetry(),
		tb.FeedingEvent(),
		tb.Alert(),
		tb.Status(),
		tb.ShadowUpdate(),
	}

	for _, topic := range topics {
		if !h.subscriptions[topic] {
			continue
		}

		if err := h.client.Unsubscribe(topic); err != nil {
			return err
		}
		delete(h.subscriptions, topic)
	}

	return nil
}

// createMessageCallback creates a callback function for handling messages on a topic
func (h *MQTTHandlers) createMessageCallback(topic string) MessageHandler {
	return func(receivedTopic string, payload []byte) error {
		ctx := context.Background()
		deviceID := ExtractDeviceID(receivedTopic)

		if deviceID == "" {
			log.Printf("Failed to extract device ID from topic: %s", receivedTopic)
			return nil
		}

		// Determine message type based on topic
		topicType := GetTopicType(receivedTopic)

		switch topicType {
		case "telemetry":
			h.handleTelemetryMessage(ctx, deviceID, payload)
		case "feeding":
			h.handleFeedingEventMessage(ctx, deviceID, payload)
		case "alert":
			h.handleAlertMessage(ctx, deviceID, payload)
		case "status":
			h.handleStatusMessage(ctx, deviceID, payload)
		case "shadow":
			h.handleShadowUpdateMessage(ctx, deviceID, payload)
		default:
			log.Printf("Unknown topic type: %s for topic: %s", topicType, receivedTopic)
		}

		return nil
	}
}

func (h *MQTTHandlers) handleTelemetryMessage(ctx context.Context, deviceID string, payload []byte) {
	telemetry := &protobuf.DeviceTelemetry{}

	// Try binary format first, then JSON
	if err := telemetry.Unmarshal(payload); err != nil {
		if err := json.Unmarshal(payload, telemetry); err != nil {
			log.Printf("Failed to unmarshal telemetry for device %s: %v", deviceID, err)
			return
		}
	}

	// Update shadow with latest telemetry
	if h.shadowService != nil {
		_, _ = h.shadowService.UpdateReportedState(ctx, deviceID, map[string]interface{}{
			"temperature":      telemetry.Temperature,
			"dissolved_oxygen": telemetry.DissolvedOxygen,
			"ph":               telemetry.PH,
			"battery_level":    telemetry.BatteryLevel,
			"status":           telemetry.Status,
		})
	}

	if h.messageHandler != nil {
		if err := h.messageHandler.HandleTelemetry(ctx, deviceID, telemetry); err != nil {
			log.Printf("Error handling telemetry for device %s: %v", deviceID, err)
		}
	}
}

func (h *MQTTHandlers) handleFeedingEventMessage(ctx context.Context, deviceID string, payload []byte) {
	event := &protobuf.FeedingEvent{}

	if err := event.Unmarshal(payload); err != nil {
		if err := json.Unmarshal(payload, event); err != nil {
			log.Printf("Failed to unmarshal feeding event for device %s: %v", deviceID, err)
			return
		}
	}

	if h.messageHandler != nil {
		if err := h.messageHandler.HandleFeedingEvent(ctx, deviceID, event); err != nil {
			log.Printf("Error handling feeding event for device %s: %v", deviceID, err)
		}
	}
}

func (h *MQTTHandlers) handleAlertMessage(ctx context.Context, deviceID string, payload []byte) {
	alert := &protobuf.DeviceAlert{}

	if err := alert.Unmarshal(payload); err != nil {
		if err := json.Unmarshal(payload, alert); err != nil {
			log.Printf("Failed to unmarshal alert for device %s: %v", deviceID, err)
			return
		}
	}

	if h.messageHandler != nil {
		if err := h.messageHandler.HandleAlert(ctx, deviceID, alert); err != nil {
			log.Printf("Error handling alert for device %s: %v", deviceID, err)
		}
	}
}

func (h *MQTTHandlers) handleStatusMessage(ctx context.Context, deviceID string, payload []byte) {
	// Status messages are typically JSON
	var status map[string]interface{}
	if err := json.Unmarshal(payload, &status); err != nil {
		log.Printf("Failed to unmarshal status for device %s: %v", deviceID, err)
		return
	}

	// Update shadow with status
	if h.shadowService != nil {
		_, _ = h.shadowService.UpdateReportedState(ctx, deviceID, status)
	}
}

func (h *MQTTHandlers) handleShadowUpdateMessage(ctx context.Context, deviceID string, payload []byte) {
	var shadowUpdate map[string]interface{}
	if err := json.Unmarshal(payload, &shadowUpdate); err != nil {
		log.Printf("Failed to unmarshal shadow update for device %s: %v", deviceID, err)
		return
	}

	if reported, ok := shadowUpdate["reported"].(map[string]interface{}); ok {
		if h.shadowService != nil {
			_, _ = h.shadowService.UpdateReportedState(ctx, deviceID, reported)
		}
	}
}

// SendCommand sends a command to a device
func (h *MQTTHandlers) SendCommand(ctx context.Context, deviceID string, cmd *protobuf.DeviceCommand) error {
	tb := NewTopicBuilder(deviceID)
	topic := tb.Command()

	payload, err := cmd.Marshal()
	if err != nil {
		return err
	}

	// Update shadow with desired state
	if h.shadowService != nil {
		_, _ = h.shadowService.UpdateDesiredState(ctx, deviceID, map[string]interface{}{
			"pending_command": cmd.CommandID,
			"command_type":    cmd.Type,
		})
	}

	return h.client.Publish(ctx, topic, payload, 1, false)
}

// SendConfig sends configuration to a device
func (h *MQTTHandlers) SendConfig(ctx context.Context, deviceID string, config *protobuf.DeviceConfig) error {
	tb := NewTopicBuilder(deviceID)
	topic := tb.Config()

	payload, err := config.Marshal()
	if err != nil {
		return err
	}

	return h.client.Publish(ctx, topic, payload, 1, true)
}

// RequestDiagnostics requests a diagnostics report from a device
func (h *MQTTHandlers) RequestDiagnostics(ctx context.Context, deviceID string) error {
	cmd := protobuf.NewDeviceCommand(deviceID, protobuf.CommandTypeRunDiagnostics)
	return h.SendCommand(ctx, deviceID, cmd)
}

// TriggerFeeding triggers an immediate feeding on a device
func (h *MQTTHandlers) TriggerFeeding(ctx context.Context, deviceID string, quantityGrams float32) error {
	cmd := protobuf.NewDeviceCommand(deviceID, protobuf.CommandTypeFeedNow)
	cmd.Payload = make([]byte, 4)
	return h.SendCommand(ctx, deviceID, cmd)
}

// StopFeeding stops an ongoing feeding on a device
func (h *MQTTHandlers) StopFeeding(ctx context.Context, deviceID string) error {
	cmd := protobuf.NewDeviceCommand(deviceID, protobuf.CommandTypeStopFeeding)
	return h.SendCommand(ctx, deviceID, cmd)
}

// CaptureImage requests a camera image capture from a device
func (h *MQTTHandlers) CaptureImage(ctx context.Context, deviceID string) error {
	cmd := protobuf.NewDeviceCommand(deviceID, protobuf.CommandTypeCaptureImage)
	return h.SendCommand(ctx, deviceID, cmd)
}

// RebootDevice reboots a device
func (h *MQTTHandlers) RebootDevice(ctx context.Context, deviceID string) error {
	cmd := protobuf.NewDeviceCommand(deviceID, protobuf.CommandTypeReboot)
	return h.SendCommand(ctx, deviceID, cmd)
}

// RunAntiJam runs the anti-jam routine on a device
func (h *MQTTHandlers) RunAntiJam(ctx context.Context, deviceID string) error {
	cmd := protobuf.NewDeviceCommand(deviceID, protobuf.CommandTypeAntiJam)
	return h.SendCommand(ctx, deviceID, cmd)
}

// GetActiveSubscriptions returns a list of active subscriptions
func (h *MQTTHandlers) GetActiveSubscriptions() []string {
	h.mu.RLock()
	defer h.mu.RUnlock()

	subs := make([]string, 0, len(h.subscriptions))
	for topic := range h.subscriptions {
		subs = append(subs, topic)
	}
	return subs
}

// DefaultDeviceMessageHandler provides a default implementation of DeviceMessageHandler
type DefaultDeviceMessageHandler struct {
	telemetryCallback       func(ctx context.Context, deviceID string, t *protobuf.DeviceTelemetry) error
	feedingEventCallback    func(ctx context.Context, deviceID string, e *protobuf.FeedingEvent) error
	alertCallback           func(ctx context.Context, deviceID string, a *protobuf.DeviceAlert) error
	commandResponseCallback func(ctx context.Context, deviceID string, r *protobuf.CommandResponse) error
	diagnosticsCallback     func(ctx context.Context, deviceID string, d *protobuf.DiagnosticsReport) error
	visionAnalysisCallback  func(ctx context.Context, deviceID string, v *protobuf.VisionAnalysis) error
}

// NewDefaultDeviceMessageHandler creates a new DefaultDeviceMessageHandler
func NewDefaultDeviceMessageHandler() *DefaultDeviceMessageHandler {
	return &DefaultDeviceMessageHandler{}
}

// OnTelemetry sets the telemetry callback
func (h *DefaultDeviceMessageHandler) OnTelemetry(cb func(ctx context.Context, deviceID string, t *protobuf.DeviceTelemetry) error) {
	h.telemetryCallback = cb
}

// OnFeedingEvent sets the feeding event callback
func (h *DefaultDeviceMessageHandler) OnFeedingEvent(cb func(ctx context.Context, deviceID string, e *protobuf.FeedingEvent) error) {
	h.feedingEventCallback = cb
}

// OnAlert sets the alert callback
func (h *DefaultDeviceMessageHandler) OnAlert(cb func(ctx context.Context, deviceID string, a *protobuf.DeviceAlert) error) {
	h.alertCallback = cb
}

// OnCommandResponse sets the command response callback
func (h *DefaultDeviceMessageHandler) OnCommandResponse(cb func(ctx context.Context, deviceID string, r *protobuf.CommandResponse) error) {
	h.commandResponseCallback = cb
}

// OnDiagnostics sets the diagnostics callback
func (h *DefaultDeviceMessageHandler) OnDiagnostics(cb func(ctx context.Context, deviceID string, d *protobuf.DiagnosticsReport) error) {
	h.diagnosticsCallback = cb
}

// OnVisionAnalysis sets the vision analysis callback
func (h *DefaultDeviceMessageHandler) OnVisionAnalysis(cb func(ctx context.Context, deviceID string, v *protobuf.VisionAnalysis) error) {
	h.visionAnalysisCallback = cb
}

// HandleTelemetry implements DeviceMessageHandler
func (h *DefaultDeviceMessageHandler) HandleTelemetry(ctx context.Context, deviceID string, telemetry *protobuf.DeviceTelemetry) error {
	if h.telemetryCallback != nil {
		return h.telemetryCallback(ctx, deviceID, telemetry)
	}
	return nil
}

// HandleFeedingEvent implements DeviceMessageHandler
func (h *DefaultDeviceMessageHandler) HandleFeedingEvent(ctx context.Context, deviceID string, event *protobuf.FeedingEvent) error {
	if h.feedingEventCallback != nil {
		return h.feedingEventCallback(ctx, deviceID, event)
	}
	return nil
}

// HandleAlert implements DeviceMessageHandler
func (h *DefaultDeviceMessageHandler) HandleAlert(ctx context.Context, deviceID string, alert *protobuf.DeviceAlert) error {
	if h.alertCallback != nil {
		return h.alertCallback(ctx, deviceID, alert)
	}
	return nil
}

// HandleCommandResponse implements DeviceMessageHandler
func (h *DefaultDeviceMessageHandler) HandleCommandResponse(ctx context.Context, deviceID string, response *protobuf.CommandResponse) error {
	if h.commandResponseCallback != nil {
		return h.commandResponseCallback(ctx, deviceID, response)
	}
	return nil
}

// HandleDiagnostics implements DeviceMessageHandler
func (h *DefaultDeviceMessageHandler) HandleDiagnostics(ctx context.Context, deviceID string, report *protobuf.DiagnosticsReport) error {
	if h.diagnosticsCallback != nil {
		return h.diagnosticsCallback(ctx, deviceID, report)
	}
	return nil
}

// HandleVisionAnalysis implements DeviceMessageHandler
func (h *DefaultDeviceMessageHandler) HandleVisionAnalysis(ctx context.Context, deviceID string, analysis *protobuf.VisionAnalysis) error {
	if h.visionAnalysisCallback != nil {
		return h.visionAnalysisCallback(ctx, deviceID, analysis)
	}
	return nil
}
