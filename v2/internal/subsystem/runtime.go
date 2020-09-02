package subsystem

import (
	"fmt"
	"strings"

	"github.com/wailsapp/wails/v2/internal/logger"
	"github.com/wailsapp/wails/v2/internal/runtime/goruntime"
	"github.com/wailsapp/wails/v2/internal/servicebus"
)

// Runtime is the Runtime subsystem. It handles messages with topics starting
// with "runtime:"
type Runtime struct {
	quitChannel    <-chan *servicebus.Message
	runtimeChannel <-chan *servicebus.Message
	running        bool

	logger logger.CustomLogger

	// Runtime library
	runtime *goruntime.Runtime
}

// NewRuntime creates a new runtime subsystem
func NewRuntime(bus *servicebus.ServiceBus, logger *logger.Logger) (*Runtime, error) {

	// Register quit channel
	quitChannel, err := bus.Subscribe("quit")
	if err != nil {
		return nil, err
	}

	// Subscribe to log messages
	runtimeChannel, err := bus.Subscribe("runtime")
	if err != nil {
		return nil, err
	}

	result := &Runtime{
		quitChannel:    quitChannel,
		runtimeChannel: runtimeChannel,
		logger:         logger.CustomLogger("Runtime Subsystem"),
		runtime:        goruntime.New(bus),
	}

	return result, nil
}

// Start the subsystem
func (r *Runtime) Start() error {

	r.running = true

	// Spin off a go routine
	go func() {
		for r.running {
			select {
			case <-r.quitChannel:
				r.running = false
				break
			case runtimeMessage := <-r.runtimeChannel:
				r.logger.Trace(fmt.Sprintf("Received message: %+v", runtimeMessage))
				// Topics have the format: "runtime:category:call"
				messageSlice := strings.Split(runtimeMessage.Topic(), ":")
				if len(messageSlice) != 3 {
					r.logger.Error("Invalid runtime message: %#v\n", runtimeMessage)
					continue
				}

				category := messageSlice[1]
				method := messageSlice[2]
				var err error
				switch category {
				case "browser":
					err = r.processBrowserMessage(method, runtimeMessage.Data())
				default:
					fmt.Errorf("unknown log message: %+v", runtimeMessage)
				}

				// If we had an error, log it
				if err != nil {
					r.logger.Error(err.Error())
				}
			}
		}

		// Call shutdown
		r.shutdown()
	}()

	return nil
}

// GoRuntime returns the Go Runtime object
func (r *Runtime) GoRuntime() *goruntime.Runtime {
	return r.runtime
}

func (r *Runtime) shutdown() {
	r.logger.Trace("Shutdown")
}

func (r *Runtime) processBrowserMessage(method string, data interface{}) error {
	switch method {
	case "openurl":
		url, ok := data.(string)
		if !ok {
			return fmt.Errorf("expected 1 string parameter for runtime:browser:openurl")
		}
		go r.runtime.Browser.Open(url)
	default:
		return fmt.Errorf("unknown method runtime:browser:%s", method)
	}
	return nil
}
