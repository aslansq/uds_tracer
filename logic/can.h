#ifndef CAN_H
#define CAN_H

#include <QThread>
#include <QObject>
#include <QMutex>
#include <QQueue>
#include <QMutexLocker>
#include <cstdint>

typedef struct
{
	uint32_t id;
	uint8_t dataLength;
	uint8_t data[64];
	uint64_t timestamp;
} CanMsg;

enum class CanEvent
{
	Connected,
	Disconnected,
	MessageReceived
};

Q_DECLARE_METATYPE(CanEvent)

template <typename T>
class ThreadSafeQueue {
public:
	void enqueue(const T& value) {
		QMutexLocker locker(&mutex);
		queue.enqueue(value);
	}

	T dequeue() {
	QMutexLocker locker(&mutex);
		if (queue.isEmpty()) {
			throw std::runtime_error("Queue is empty");
		}
		return queue.dequeue();
	}

	bool isEmpty() const {
		QMutexLocker locker(&mutex);
		return queue.isEmpty();
	}

private:
	QQueue<T> queue;
	mutable QMutex mutex;
};


class Can : public QObject
{
	Q_OBJECT
public:
	Can(QObject *parent);
	virtual ~Can() {}

	virtual void connect(const void * configPtr) = 0;
	virtual void disconnect(void) = 0;
	virtual void rx(void) = 0;

	bool isConnected(void) const;
	static void printMsg(const CanMsg &canMsgRef);
	static QString getMsgStr(const CanMsg &canMsgRef);
	bool getCanMsgFromRaw(const uint8_t *rawCanMsgPtr, size_t rawCanMsgSize, CanMsg &canMsgRef);
	static size_t getRawCanMsg(const CanMsg &canMsgRef, uint8_t *rawCanMsgPtr, size_t rawCanMsgSize);

	ThreadSafeQueue<CanMsg> rxQueue;
signals:
	void eventOccured(CanEvent event);
protected:
	void stopRxThread(void);
	void startRxThread(void);
	QThread *rxThread;
	CanMsg canMsg;
};

#endif // CAN_H
