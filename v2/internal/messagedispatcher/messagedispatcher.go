package messagedispatcher

import (
	"encoding/json"
	"strings"
	"sync"

	"github.com/leaanthony/wailsv2/v2/internal/crypto"
	"github.com/leaanthony/wailsv2/v2/internal/logger"
	"github.com/leaanthony/wailsv2/v2/internal/messagedispatcher/message"
	"github.com/leaanthony/wailsv2/v2/internal/servicebus"
)

// Dispatcher translates messages received from the frontend
// and publishes them onto the service bus
type Dispatcher struct {
	quitChannel   <-chan *servicebus.Message
	resultChannel <-chan *servicebus.Message
	eventChannel  <-chan *servicebus.Message
	windowChannel <-chan *servicebus.Message
	dialogChannel <-chan *servicebus.Message
	running       bool

	servicebus *servicebus.ServiceBus
	logger     logger.CustomLogger

	// Clients
	clients map[string]*DispatchClient
	lock    sync.RWMutex
}

// New dispatcher. Needs a service bus to send to.
func New(servicebus *servicebus.ServiceBus, logger *logger.Logger) (*Dispatcher, error) {
	// Subscribe to call result messages
	resultChannel, err := servicebus.Subscribe("call:result")
	if err != nil {
		return nil, err
	}

	// Subscribe to event messages
	eventChannel, err := servicebus.Subscribe("event:emit")
	if err != nil {
		return nil, err
	}

	// Subscribe to quit messages
	quitChannel, err := servicebus.Subscribe("quit")
	if err != nil {
		return nil, err
	}

	// Subscribe to window messages
	windowChannel, err := servicebus.Subscribe("window")
	if err != nil {
		return nil, err
	}

	// Subscribe to dialog events
	dialogChannel, err := servicebus.Subscribe("dialog:select")
	if err != nil {
		return nil, err
	}

	result := &Dispatcher{
		servicebus:    servicebus,
		eventChannel:  eventChannel,
		logger:        logger.CustomLogger("Message Dispatcher"),
		clients:       make(map[string]*DispatchClient),
		resultChannel: resultChannel,
		quitChannel:   quitChannel,
		windowChannel: windowChannel,
		dialogChannel: dialogChannel,
	}

	return result, nil
}

// Start the subsystem
func (d *Dispatcher) Start() error {

	d.logger.Trace("Starting")

	d.running = true

	// Spin off a go routine
	go func() {
		for d.running {
			select {
			case <-d.quitChannel:
				d.processQuit()
				d.running = false
			case resultMessage := <-d.resultChannel:
				d.processCallResult(resultMessage)
			case eventMessage := <-d.eventChannel:
				d.processEvent(eventMessage)
			case windowMessage := <-d.windowChannel:
				d.processWindowMessage(windowMessage)
			case dialogMessage := <-d.dialogChannel:
				d.processDialogMessage(dialogMessage)
			}
		}

		// Call shutdown
		d.shutdown()
	}()

	return nil
}

func (d *Dispatcher) processQuit() {
	d.lock.RLock()
	defer d.lock.RUnlock()
	for _, client := range d.clients {
		client.frontend.Quit()
	}
}

func (d *Dispatcher) shutdown() {
	d.logger.Trace("Shutdown")
}

// RegisterClient will register the given callback with the dispatcher
// and return a DispatchClient that the caller can use to send messages
func (d *Dispatcher) RegisterClient(client Client) *DispatchClient {
	d.lock.Lock()
	defer d.lock.Unlock()

	// Create ID
	id := d.getUniqueID()
	d.clients[id] = newDispatchClient(id, client, d.logger, d.servicebus)

	return d.clients[id]
}

// RemoveClient will remove the registered client
func (d *Dispatcher) RemoveClient(dc *DispatchClient) {
	d.lock.Lock()
	defer d.lock.Unlock()
	delete(d.clients, dc.id)
}

func (d *Dispatcher) getUniqueID() string {
	var uid string
	for {
		uid = crypto.RandomID()

		if d.clients[uid] == nil {
			break
		}
	}

	return uid
}

func (d *Dispatcher) processCallResult(result *servicebus.Message) {
	target := result.Target()

	if target == "" {
		// This is an error. Calls are 1:1!
		d.logger.Fatal("No target for call result: %+v", result)
	}

	d.lock.RLock()
	client := d.clients[target]
	d.lock.RUnlock()
	if client == nil {
		// This is fatal - unknown target!
		d.logger.Fatal("Unknown target for call result: %+v", result)
	}

	d.logger.Trace("Sending message to client %s: R%s", target, result.Data().(string))
	client.frontend.CallResult(result.Data().(string))
}

// processEvent will
func (d *Dispatcher) processEvent(result *servicebus.Message) {

	d.logger.Trace("Got event in message dispatcher: %+v", result)

	splitTopic := strings.Split(result.Topic(), ":")
	eventType := splitTopic[1]
	switch eventType {
	case "emit":
		eventFrom := splitTopic[3]
		if eventFrom == "g" {
			// This was sent from Go - notify frontend
			eventData := result.Data().(*message.EventMessage)
			// Unpack event
			payload, err := json.Marshal(eventData)
			if err != nil {
				d.logger.Error("Unable to marshal eventData: %s", err.Error())
				return
			}
			d.lock.RLock()
			for _, client := range d.clients {
				client.frontend.NotifyEvent(string(payload))
			}
			d.lock.RUnlock()
		}
	default:
		d.logger.Error("Unknown event type: %s", eventType)
	}
}

// processWindowMessage processes messages intended for the window
func (d *Dispatcher) processWindowMessage(result *servicebus.Message) {
	d.lock.RLock()
	defer d.lock.RUnlock()
	splitTopic := strings.Split(result.Topic(), ":")
	command := splitTopic[1]
	switch command {
	case "settitle":
		title, ok := result.Data().(string)
		if !ok {
			d.logger.Error("Invalid title for 'window:settitle' : %#v", result.Data())
			return
		}
		// Notify clients
		for _, client := range d.clients {
			client.frontend.WindowSetTitle(title)
		}
	case "fullscreen":
		// Notify clients
		for _, client := range d.clients {
			client.frontend.WindowFullscreen()
		}
	case "unfullscreen":
		// Notify clients
		for _, client := range d.clients {
			client.frontend.WindowUnFullscreen()
		}
	case "setcolour":
		colour, ok := result.Data().(string)
		if !ok {
			d.logger.Error("Invalid colour for 'window:setcolour' : %#v", result.Data())
			return
		}
		// Notify clients
		for _, client := range d.clients {
			client.frontend.WindowSetColour(colour)
		}
	default:
		d.logger.Error("Unknown window command: %s", command)
	}
	d.logger.Trace("Got window in message dispatcher: %+v", result)

}

// processDialogMessage processes dialog messages
func (d *Dispatcher) processDialogMessage(result *servicebus.Message) {
	splitTopic := strings.Split(result.Topic(), ":")

	if len(splitTopic) < 4 {
		d.logger.Error("Invalid dialog message : %#v", result.Data())
		return
	}

	command := splitTopic[1]
	switch command {
	case "select":
		dialogType := splitTopic[2]
		title := splitTopic[3]
		switch dialogType {
		case "file":
			responseTopic, ok := result.Data().(string)
			if !ok {
				d.logger.Error("Invalid responseTopic for 'dialog:select:file' : %#v", result.Data())
				return
			}
			d.logger.Info("Opening File dialog! responseTopic = %s", responseTopic)

			// TODO: Work out what we mean in a multi window environment...
			// For now we will just pick the first one
			var result string
			for _, client := range d.clients {
				result = client.frontend.OpenFileDialog(title)
			}

			// Send dummy response
			d.servicebus.Publish(responseTopic, result)
		case "filesave":
			responseTopic, ok := result.Data().(string)
			if !ok {
				d.logger.Error("Invalid responseTopic for 'dialog:select:filesave' : %#v", result.Data())
				return
			}
			d.logger.Info("Opening Save File dialog! responseTopic = %s", responseTopic)

			// TODO: Work out what we mean in a multi window environment...
			// For now we will just pick the first one
			var result string
			for _, client := range d.clients {
				result = client.frontend.SaveFileDialog(title)
			}

			// Send dummy response
			d.servicebus.Publish(responseTopic, result)
		case "directory":
			responseTopic, ok := result.Data().(string)
			if !ok {
				d.logger.Error("Invalid responseTopic for 'dialog:select:directory' : %#v", result.Data())
				return
			}
			d.logger.Info("Opening Directory dialog! responseTopic = %s", responseTopic)

			// TODO: Work out what we mean in a multi window environment...
			// For now we will just pick the first one
			var result string
			for _, client := range d.clients {
				result = client.frontend.OpenDirectoryDialog(title)
			}
			// Send dummy response
			d.servicebus.Publish(responseTopic, result)

		default:
			d.logger.Error("Unknown dialog command: %s", command)
		}
	}
}
