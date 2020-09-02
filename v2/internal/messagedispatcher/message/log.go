package message

import "fmt"

var logMessageMap = map[byte]string{
	'D': "log:debug",
	'I': "log:info",
	'W': "log:warning",
	'E': "log:error",
	'F': "log:fatal",
}

// logMessageParser does what it says on the tin!
func logMessageParser(message string) (*parsedMessage, error) {
	
	// Sanity check: Log messages must be at least 2 bytes
	if len(message) < 2 {
		return nil, fmt.Errorf("log message was an invalid length")
	}

	// Switch on the log type
	messageTopic := logMessageMap[message[1]]

	// If the type is invalid, raise error
	if messageTopic == "" {
		return nil, fmt.Errorf("log message type '%b' invalid", message[1])
	}

	// Create a new parsed message struct
	parsedMessage := &parsedMessage{Topic: messageTopic, Data: message[2:]}

	return parsedMessage, nil
}
