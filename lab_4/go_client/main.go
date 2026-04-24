package main

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"math/rand"
	"net"
	"os"
	"time"
)

const (
	magic   uint32 = 0x4D4D5831
	version uint16 = 1

	MSG_DATA        uint16 = 1
	MSG_START       uint16 = 2
	MSG_STATUS      uint16 = 3
	MSG_DATA_OK     uint16 = 101
	MSG_START_OK    uint16 = 102
	MSG_STATUS_RESP uint16 = 103
	MSG_ERROR       uint16 = 200

	STATUS_IDLE    uint32 = 0
	STATUS_READY   uint32 = 1
	STATUS_RUNNING uint32 = 2
	STATUS_DONE    uint32 = 3
	STATUS_ERROR   uint32 = 4
)

type MessageHeader struct {
	Magic   uint32
	Version uint16
	Type    uint16
	Length  uint32
}

func writeHeader(buf *bytes.Buffer, typ uint16, length uint32) {
	binary.Write(buf, binary.BigEndian, magic)
	binary.Write(buf, binary.BigEndian, version)
	binary.Write(buf, binary.BigEndian, typ)
	binary.Write(buf, binary.BigEndian, length)
}

func sendAll(conn net.Conn, data []byte) error {
	total := 0
	for total < len(data) {
		n, err := conn.Write(data[total:])
		if err != nil {
			return err
		}
		total += n
	}
	return nil
}

func recvAll(conn net.Conn, size int) ([]byte, error) {
	buf := make([]byte, size)
	total := 0
	for total < size {
		n, err := conn.Read(buf[total:])
		if err != nil {
			return nil, err
		}
		total += n
	}
	return buf, nil
}

func sendData(conn net.Conn, rows, cols, numThreads uint32, matrix []int32) error {
	buf := &bytes.Buffer{}
	writeHeader(buf, MSG_DATA, uint32(12+4*len(matrix)))
	binary.Write(buf, binary.BigEndian, rows)
	binary.Write(buf, binary.BigEndian, cols)
	binary.Write(buf, binary.BigEndian, numThreads)
	for _, v := range matrix {
		binary.Write(buf, binary.BigEndian, v)
	}
	return sendAll(conn, buf.Bytes())
}

func sendSimple(conn net.Conn, typ uint16) error {
	buf := &bytes.Buffer{}
	writeHeader(buf, typ, 0)
	return sendAll(conn, buf.Bytes())
}

func recvHeader(conn net.Conn) (MessageHeader, error) {
	hdr := MessageHeader{}
	data, err := recvAll(conn, 12)
	if err != nil {
		return hdr, err
	}
	buf := bytes.NewReader(data)
	binary.Read(buf, binary.BigEndian, &hdr.Magic)
	binary.Read(buf, binary.BigEndian, &hdr.Version)
	binary.Read(buf, binary.BigEndian, &hdr.Type)
	binary.Read(buf, binary.BigEndian, &hdr.Length)
	return hdr, nil
}

func main() {
	host := "127.0.0.1"
	port := 5000
	rows := uint32(100)
	cols := uint32(100)
	threads := uint32(4)

	matrix := make([]int32, int(rows*cols))
	rand.Seed(time.Now().UnixNano())
	for i := range matrix {
		matrix[i] = rand.Int31n(2001) - 1000
	}

	addr := fmt.Sprintf("%s:%d", host, port)
	conn, err := net.Dial("tcp", addr)
	if err != nil {
		fmt.Println("[go-client] connect error:", err)
		os.Exit(1)
	}
	defer conn.Close()

	if err := sendData(conn, rows, cols, threads, matrix); err != nil {
		fmt.Println("[go-client] send DATA error:", err)
		return
	}
	hdr, err := recvHeader(conn)
	if err != nil {
		fmt.Println("[go-client] recv DATA_OK error:", err)
		return
	}
	if hdr.Type == MSG_ERROR {
		payload, _ := recvAll(conn, int(hdr.Length))
		fmt.Println("[go-client] server error:", payload)
		return
	}
	if hdr.Type != MSG_DATA_OK {
		fmt.Println("[go-client] unexpected DATA_OK resp:", hdr.Type)
		return
	}

	if err := sendSimple(conn, MSG_START); err != nil {
		fmt.Println("[go-client] send START error:", err)
		return
	}
	hdr, err = recvHeader(conn)
	if err != nil {
		fmt.Println("[go-client] recv START_OK error:", err)
		return
	}
	if hdr.Type != MSG_START_OK {
		fmt.Println("[go-client] unexpected START_OK resp:", hdr.Type)
		return
	}

	for {
		if err := sendSimple(conn, MSG_STATUS); err != nil {
			fmt.Println("[go-client] send STATUS error:", err)
			return
		}
		hdr, err := recvHeader(conn)
		if err != nil {
			fmt.Println("[go-client] recv STATUS_RESP error:", err)
			return
		}
		if hdr.Type == MSG_ERROR {
			payload, _ := recvAll(conn, int(hdr.Length))
			fmt.Println("[go-client] server error:", payload)
			return
		}
		if hdr.Type != MSG_STATUS_RESP {
			fmt.Println("[go-client] unexpected STATUS_RESP resp:", hdr.Type)
			return
		}
		payload, err := recvAll(conn, int(hdr.Length))
		if err != nil {
			fmt.Println("[go-client] recv STATUS_RESP payload error:", err)
			return
		}
		if len(payload) < 16 {
			fmt.Println("[go-client] STATUS_RESP payload too short")
			return
		}
		status := binary.BigEndian.Uint32(payload[0:4])
		min := int32(binary.BigEndian.Uint32(payload[4:8]))
		max := int32(binary.BigEndian.Uint32(payload[8:12]))
		errCode := binary.BigEndian.Uint32(payload[12:16])
		if status == STATUS_DONE {
			fmt.Printf("[go-client] result: min=%d, max=%d\n", min, max)
			break
		}
		if status == STATUS_ERROR {
			fmt.Printf("[go-client] server error code: %d\n", errCode)
			break
		}
		time.Sleep(100 * time.Millisecond)
	}
}
