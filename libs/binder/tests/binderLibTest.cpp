/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include <gtest/gtest.h>

#include <binder/Binder.h>
#include <binder/IBinder.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>

#define ARRAY_SIZE(array) (sizeof array / sizeof array[0])

using namespace android;

static testing::Environment* binder_env;
static char *binderservername;
static char binderserverarg[] = "--binderserver";

static String16 binderLibTestServiceName = String16("test.binderLib");

enum BinderLibTestTranscationCode {
    BINDER_LIB_TEST_NOP_TRANSACTION = IBinder::FIRST_CALL_TRANSACTION,
    BINDER_LIB_TEST_REGISTER_SERVER,
    BINDER_LIB_TEST_ADD_SERVER,
    BINDER_LIB_TEST_CALL_BACK,
    BINDER_LIB_TEST_NOP_CALL_BACK,
    BINDER_LIB_TEST_GET_ID_TRANSACTION,
    BINDER_LIB_TEST_INDIRECT_TRANSACTION,
    BINDER_LIB_TEST_SET_ERROR_TRANSACTION,
    BINDER_LIB_TEST_GET_STATUS_TRANSACTION,
    BINDER_LIB_TEST_ADD_STRONG_REF_TRANSACTION,
    BINDER_LIB_TEST_LINK_DEATH_TRANSACTION,
    BINDER_LIB_TEST_WRITE_FILE_TRANSACTION,
    BINDER_LIB_TEST_PROMOTE_WEAK_REF_TRANSACTION,
    BINDER_LIB_TEST_EXIT_TRANSACTION,
    BINDER_LIB_TEST_DELAYED_EXIT_TRANSACTION,
    BINDER_LIB_TEST_GET_PTR_SIZE_TRANSACTION,
};

pid_t start_server_process(int arg2)
{
    int ret;
    pid_t pid;
    status_t status;
    int pipefd[2];
    char stri[16];
    char strpipefd1[16];
    char *childargv[] = {
        binderservername,
        binderserverarg,
        stri,
        strpipefd1,
        NULL
    };

    ret = pipe(pipefd);
    if (ret < 0)
        return ret;

    snprintf(stri, sizeof(stri), "%d", arg2);
    snprintf(strpipefd1, sizeof(strpipefd1), "%d", pipefd[1]);

    pid = fork();
    if (pid == -1)
        return pid;
    if (pid == 0) {
        close(pipefd[0]);
        execv(binderservername, childargv);
        status = -errno;
        write(pipefd[1], &status, sizeof(status));
        fprintf(stderr, "execv failed, %s\n", strerror(errno));
        _exit(EXIT_FAILURE);
    }
    close(pipefd[1]);
    ret = read(pipefd[0], &status, sizeof(status));
    //printf("pipe read returned %d, status %d\n", ret, status);
    close(pipefd[0]);
    if (ret == sizeof(status)) {
        ret = status;
    } else {
        kill(pid, SIGKILL);
        if (ret >= 0) {
            ret = NO_INIT;
        }
    }
    if (ret < 0) {
        wait(NULL);
        return ret;
    }
    return pid;
}

class BinderLibTestEnv : public ::testing::Environment {
    public:
        BinderLibTestEnv() {}
        sp<IBinder> getServer(void) {
            return m_server;
        }

    private:
        virtual void SetUp() {
            m_serverpid = start_server_process(0);
            //printf("m_serverpid %d\n", m_serverpid);
            ASSERT_GT(m_serverpid, 0);

            sp<IServiceManager> sm = defaultServiceManager();
            //printf("%s: pid %d, get service\n", __func__, m_pid);
            m_server = sm->getService(binderLibTestServiceName);
            ASSERT_TRUE(m_server != NULL);
            //printf("%s: pid %d, get service done\n", __func__, m_pid);
        }
        virtual void TearDown() {
            status_t ret;
            Parcel data, reply;
            int exitStatus;
            pid_t pid;

            //printf("%s: pid %d\n", __func__, m_pid);
            if (m_server != NULL) {
                ret = m_server->transact(BINDER_LIB_TEST_GET_STATUS_TRANSACTION, data, &reply);
                EXPECT_EQ(0, ret);
                ret = m_server->transact(BINDER_LIB_TEST_EXIT_TRANSACTION, data, &reply, TF_ONE_WAY);
                EXPECT_EQ(0, ret);
            }
            if (m_serverpid > 0) {
                //printf("wait for %d\n", m_pids[i]);
                pid = wait(&exitStatus);
                EXPECT_EQ(m_serverpid, pid);
                EXPECT_TRUE(WIFEXITED(exitStatus));
                EXPECT_EQ(0, WEXITSTATUS(exitStatus));
            }
        }

        pid_t m_serverpid;
        sp<IBinder> m_server;
};

class BinderLibTest : public ::testing::Test {
    public:
        virtual void SetUp() {
            m_server = static_cast<BinderLibTestEnv *>(binder_env)->getServer();
        }
        virtual void TearDown() {
        }
    protected:
        sp<IBinder> addServer(int32_t *idPtr = NULL)
        {
            int ret;
            int32_t id;
            Parcel data, reply;
            sp<IBinder> binder;

            ret = m_server->transact(BINDER_LIB_TEST_ADD_SERVER, data, &reply);
            EXPECT_EQ(NO_ERROR, ret);

            EXPECT_FALSE(binder != NULL);
            binder = reply.readStrongBinder();
            EXPECT_TRUE(binder != NULL);
            ret = reply.readInt32(&id);
            EXPECT_EQ(NO_ERROR, ret);
            if (idPtr)
                *idPtr = id;
            return binder;
        }
        void waitForReadData(int fd, int timeout_ms) {
            int ret;
            pollfd pfd = pollfd();

            pfd.fd = fd;
            pfd.events = POLLIN;
            ret = poll(&pfd, 1, timeout_ms);
            EXPECT_EQ(1, ret);
        }

        sp<IBinder> m_server;
};

class BinderLibTestBundle : public Parcel
{
    public:
        BinderLibTestBundle(void) {}
        BinderLibTestBundle(const Parcel *source) : m_isValid(false) {
            int32_t mark;
            int32_t bundleLen;
            size_t pos;

            if (source->readInt32(&mark))
                return;
            if (mark != MARK_START)
                return;
            if (source->readInt32(&bundleLen))
                return;
            pos = source->dataPosition();
            if (Parcel::appendFrom(source, pos, bundleLen))
                return;
            source->setDataPosition(pos + bundleLen);
            if (source->readInt32(&mark))
                return;
            if (mark != MARK_END)
                return;
            m_isValid = true;
            setDataPosition(0);
        }
        void appendTo(Parcel *dest) {
            dest->writeInt32(MARK_START);
            dest->writeInt32(dataSize());
            dest->appendFrom(this, 0, dataSize());
            dest->writeInt32(MARK_END);
        };
        bool isValid(void) {
            return m_isValid;
        }
    private:
        enum {
            MARK_START  = B_PACK_CHARS('B','T','B','S'),
            MARK_END    = B_PACK_CHARS('B','T','B','E'),
        };
        bool m_isValid;
};

class BinderLibTestEvent
{
    public:
        BinderLibTestEvent(void)
            : m_eventTriggered(false)
        {
            pthread_mutex_init(&m_waitMutex, NULL);
            pthread_cond_init(&m_waitCond, NULL);
        }
        int waitEvent(int timeout_s)
        {
            int ret;
            pthread_mutex_lock(&m_waitMutex);
            if (!m_eventTriggered) {
#if defined(HAVE_PTHREAD_COND_TIMEDWAIT_RELATIVE)
                pthread_cond_timeout_np(&m_waitCond, &m_waitMutex, timeout_s * 1000);
#else
                struct timespec ts;
                clock_gettime(CLOCK_REALTIME, &ts);
                ts.tv_sec += timeout_s;
                pthread_cond_timedwait(&m_waitCond, &m_waitMutex, &ts);
#endif
            }
            ret = m_eventTriggered ? NO_ERROR : TIMED_OUT;
            pthread_mutex_unlock(&m_waitMutex);
            return ret;
        }
    protected:
        void triggerEvent(void) {
            pthread_mutex_lock(&m_waitMutex);
            pthread_cond_signal(&m_waitCond);
            m_eventTriggered = true;
            pthread_mutex_unlock(&m_waitMutex);
        };
    private:
        pthread_mutex_t m_waitMutex;
        pthread_cond_t m_waitCond;
        bool m_eventTriggered;
};

class BinderLibTestCallBack : public BBinder, public BinderLibTestEvent
{
    public:
        BinderLibTestCallBack()
            : m_result(NOT_ENOUGH_DATA)
        {
        }
        status_t getResult(void)
        {
            return m_result;
        }

    private:
        virtual status_t onTransact(uint32_t code,
                                    const Parcel& data, Parcel* reply,
                                    uint32_t flags = 0)
        {
            (void)reply;
            (void)flags;
            switch(code) {
            case BINDER_LIB_TEST_CALL_BACK:
                m_result = data.readInt32();
                triggerEvent();
                return NO_ERROR;
            default:
                return UNKNOWN_TRANSACTION;
            }
        }

        status_t m_result;
};

class TestDeathRecipient : public IBinder::DeathRecipient, public BinderLibTestEvent
{
    private:
        virtual void binderDied(const wp<IBinder>& who) {
            (void)who;
            triggerEvent();
        };
};

TEST_F(BinderLibTest, NopTransaction) {
    status_t ret;
    Parcel data, reply;
    ret = m_server->transact(BINDER_LIB_TEST_NOP_TRANSACTION, data, &reply);
    EXPECT_EQ(NO_ERROR, ret);
}

TEST_F(BinderLibTest, SetError) {
    int32_t testValue[] = { 0, -123, 123 };
    for (size_t i = 0; i < ARRAY_SIZE(testValue); i++) {
        status_t ret;
        Parcel data, reply;
        data.writeInt32(testValue[i]);
        ret = m_server->transact(BINDER_LIB_TEST_SET_ERROR_TRANSACTION, data, &reply);
        EXPECT_EQ(testValue[i], ret);
    }
}

TEST_F(BinderLibTest, GetId) {
    status_t ret;
    int32_t id;
    Parcel data, reply;
    ret = m_server->transact(BINDER_LIB_TEST_GET_ID_TRANSACTION, data, &reply);
    EXPECT_EQ(NO_ERROR, ret);
    ret = reply.readInt32(&id);
    EXPECT_EQ(NO_ERROR, ret);
    EXPECT_EQ(0, id);
}

TEST_F(BinderLibTest, PtrSize) {
    status_t ret;
    int32_t ptrsize;
    Parcel data, reply;
    sp<IBinder> server = addServer();
    ASSERT_TRUE(server != NULL);
    ret = server->transact(BINDER_LIB_TEST_GET_PTR_SIZE_TRANSACTION, data, &reply);
    EXPECT_EQ(NO_ERROR, ret);
    ret = reply.readInt32(&ptrsize);
    EXPECT_EQ(NO_ERROR, ret);
    RecordProperty("TestPtrSize", sizeof(void *));
    RecordProperty("ServerPtrSize", sizeof(void *));
}

TEST_F(BinderLibTest, IndirectGetId2)
{
    status_t ret;
    int32_t id;
    int32_t count;
    Parcel data, reply;
    int32_t serverId[3];

    data.writeInt32(ARRAY_SIZE(serverId));
    for (size_t i = 0; i < ARRAY_SIZE(serverId); i++) {
        sp<IBinder> server;
        BinderLibTestBundle datai;

        server = addServer(&serverId[i]);
        ASSERT_TRUE(server != NULL);
        data.writeStrongBinder(server);
        data.writeInt32(BINDER_LIB_TEST_GET_ID_TRANSACTION);
        datai.appendTo(&data);
    }

    ret = m_server->transact(BINDER_LIB_TEST_INDIRECT_TRANSACTION, data, &reply);
    ASSERT_EQ(NO_ERROR, ret);

    ret = reply.readInt32(&id);
    ASSERT_EQ(NO_ERROR, ret);
    EXPECT_EQ(0, id);

    ret = reply.readInt32(&count);
    ASSERT_EQ(NO_ERROR, ret);
    EXPECT_EQ(ARRAY_SIZE(serverId), count);

    for (size_t i = 0; i < (size_t)count; i++) {
        BinderLibTestBundle replyi(&reply);
        EXPECT_TRUE(replyi.isValid());
        ret = replyi.readInt32(&id);
        EXPECT_EQ(NO_ERROR, ret);
        EXPECT_EQ(serverId[i], id);
        EXPECT_EQ(replyi.dataSize(), replyi.dataPosition());
    }

    EXPECT_EQ(reply.dataSize(), reply.dataPosition());
}

TEST_F(BinderLibTest, IndirectGetId3)
{
    status_t ret;
    int32_t id;
    int32_t count;
    Parcel data, reply;
    int32_t serverId[3];

    data.writeInt32(ARRAY_SIZE(serverId));
    for (size_t i = 0; i < ARRAY_SIZE(serverId); i++) {
        sp<IBinder> server;
        BinderLibTestBundle datai;
        BinderLibTestBundle datai2;

        server = addServer(&serverId[i]);
        ASSERT_TRUE(server != NULL);
        data.writeStrongBinder(server);
        data.writeInt32(BINDER_LIB_TEST_INDIRECT_TRANSACTION);

        datai.writeInt32(1);
        datai.writeStrongBinder(m_server);
        datai.writeInt32(BINDER_LIB_TEST_GET_ID_TRANSACTION);
        datai2.appendTo(&datai);

        datai.appendTo(&data);
    }

    ret = m_server->transact(BINDER_LIB_TEST_INDIRECT_TRANSACTION, data, &reply);
    ASSERT_EQ(NO_ERROR, ret);

    ret = reply.readInt32(&id);
    ASSERT_EQ(NO_ERROR, ret);
    EXPECT_EQ(0, id);

    ret = reply.readInt32(&count);
    ASSERT_EQ(NO_ERROR, ret);
    EXPECT_EQ(ARRAY_SIZE(serverId), count);

    for (size_t i = 0; i < (size_t)count; i++) {
        int32_t counti;

        BinderLibTestBundle replyi(&reply);
        EXPECT_TRUE(replyi.isValid());
        ret = replyi.readInt32(&id);
        EXPECT_EQ(NO_ERROR, ret);
        EXPECT_EQ(serverId[i], id);

        ret = replyi.readInt32(&counti);
        ASSERT_EQ(NO_ERROR, ret);
        EXPECT_EQ(1, counti);

        BinderLibTestBundle replyi2(&replyi);
        EXPECT_TRUE(replyi2.isValid());
        ret = replyi2.readInt32(&id);
        EXPECT_EQ(NO_ERROR, ret);
        EXPECT_EQ(0, id);
        EXPECT_EQ(replyi2.dataSize(), replyi2.dataPosition());

        EXPECT_EQ(replyi.dataSize(), replyi.dataPosition());
    }

    EXPECT_EQ(reply.dataSize(), reply.dataPosition());
}

TEST_F(BinderLibTest, CallBack)
{
    status_t ret;
    Parcel data, reply;
    sp<BinderLibTestCallBack> callBack = new BinderLibTestCallBack();
    data.writeStrongBinder(callBack);
    ret = m_server->transact(BINDER_LIB_TEST_NOP_CALL_BACK, data, &reply, TF_ONE_WAY);
    EXPECT_EQ(NO_ERROR, ret);
    ret = callBack->waitEvent(5);
    EXPECT_EQ(NO_ERROR, ret);
    ret = callBack->getResult();
    EXPECT_EQ(NO_ERROR, ret);
}

TEST_F(BinderLibTest, AddServer)
{
    sp<IBinder> server = addServer();
    ASSERT_TRUE(server != NULL);
}

TEST_F(BinderLibTest, DeathNotificationNoRefs)
{
    status_t ret;

    sp<TestDeathRecipient> testDeathRecipient = new TestDeathRecipient();

    {
        sp<IBinder> binder = addServer();
        ASSERT_TRUE(binder != NULL);
        ret = binder->linkToDeath(testDeathRecipient);
        EXPECT_EQ(NO_ERROR, ret);
    }
    IPCThreadState::self()->flushCommands();
    ret = testDeathRecipient->waitEvent(5);
    EXPECT_EQ(NO_ERROR, ret);
#if 0 /* Is there an unlink api that does not require a strong reference? */
    ret = binder->unlinkToDeath(testDeathRecipient);
    EXPECT_EQ(NO_ERROR, ret);
#endif
}

TEST_F(BinderLibTest, DeathNotificationWeakRef)
{
    status_t ret;
    wp<IBinder> wbinder;

    sp<TestDeathRecipient> testDeathRecipient = new TestDeathRecipient();

    {
        sp<IBinder> binder = addServer();
        ASSERT_TRUE(binder != NULL);
        ret = binder->linkToDeath(testDeathRecipient);
        EXPECT_EQ(NO_ERROR, ret);
        wbinder = binder;
    }
    IPCThreadState::self()->flushCommands();
    ret = testDeathRecipient->waitEvent(5);
    EXPECT_EQ(NO_ERROR, ret);
#if 0 /* Is there an unlink api that does not require a strong reference? */
    ret = binder->unlinkToDeath(testDeathRecipient);
    EXPECT_EQ(NO_ERROR, ret);
#endif
}

TEST_F(BinderLibTest, DeathNotificationStrongRef)
{
    status_t ret;
    sp<IBinder> sbinder;

    sp<TestDeathRecipient> testDeathRecipient = new TestDeathRecipient();

    {
        sp<IBinder> binder = addServer();
        ASSERT_TRUE(binder != NULL);
        ret = binder->linkToDeath(testDeathRecipient);
        EXPECT_EQ(NO_ERROR, ret);
        sbinder = binder;
    }
    {
        Parcel data, reply;
        ret = sbinder->transact(BINDER_LIB_TEST_EXIT_TRANSACTION, data, &reply, TF_ONE_WAY);
        EXPECT_EQ(0, ret);
    }
    IPCThreadState::self()->flushCommands();
    ret = testDeathRecipient->waitEvent(5);
    EXPECT_EQ(NO_ERROR, ret);
    ret = sbinder->unlinkToDeath(testDeathRecipient);
    EXPECT_EQ(DEAD_OBJECT, ret);
}

TEST_F(BinderLibTest, DeathNotificationMultiple)
{
    status_t ret;
    const int clientcount = 2;
    sp<IBinder> target;
    sp<IBinder> linkedclient[clientcount];
    sp<BinderLibTestCallBack> callBack[clientcount];
    sp<IBinder> passiveclient[clientcount];

    target = addServer();
    ASSERT_TRUE(target != NULL);
    for (int i = 0; i < clientcount; i++) {
        {
            Parcel data, reply;

            linkedclient[i] = addServer();
            ASSERT_TRUE(linkedclient[i] != NULL);
            callBack[i] = new BinderLibTestCallBack();
            data.writeStrongBinder(target);
            data.writeStrongBinder(callBack[i]);
            ret = linkedclient[i]->transact(BINDER_LIB_TEST_LINK_DEATH_TRANSACTION, data, &reply, TF_ONE_WAY);
            EXPECT_EQ(NO_ERROR, ret);
        }
        {
            Parcel data, reply;

            passiveclient[i] = addServer();
            ASSERT_TRUE(passiveclient[i] != NULL);
            data.writeStrongBinder(target);
            ret = passiveclient[i]->transact(BINDER_LIB_TEST_ADD_STRONG_REF_TRANSACTION, data, &reply, TF_ONE_WAY);
            EXPECT_EQ(NO_ERROR, ret);
        }
    }
    {
        Parcel data, reply;
        ret = target->transact(BINDER_LIB_TEST_EXIT_TRANSACTION, data, &reply, TF_ONE_WAY);
        EXPECT_EQ(0, ret);
    }

    for (int i = 0; i < clientcount; i++) {
        ret = callBack[i]->waitEvent(5);
        EXPECT_EQ(NO_ERROR, ret);
        ret = callBack[i]->getResult();
        EXPECT_EQ(NO_ERROR, ret);
    }
}

TEST_F(BinderLibTest, PassFile) {
    int ret;
    int pipefd[2];
    uint8_t buf[1] = { 0 };
    uint8_t write_value = 123;

    ret = pipe2(pipefd, O_NONBLOCK);
    ASSERT_EQ(0, ret);

    {
        Parcel data, reply;
        uint8_t writebuf[1] = { write_value };

        ret = data.writeFileDescriptor(pipefd[1], true);
        EXPECT_EQ(NO_ERROR, ret);

        ret = data.writeInt32(sizeof(writebuf));
        EXPECT_EQ(NO_ERROR, ret);

        ret = data.write(writebuf, sizeof(writebuf));
        EXPECT_EQ(NO_ERROR, ret);

        ret = m_server->transact(BINDER_LIB_TEST_WRITE_FILE_TRANSACTION, data, &reply);
        EXPECT_EQ(NO_ERROR, ret);
    }

    ret = read(pipefd[0], buf, sizeof(buf));
    EXPECT_EQ(sizeof(buf), ret);
    EXPECT_EQ(write_value, buf[0]);

    waitForReadData(pipefd[0], 5000); /* wait for other proccess to close pipe */

    ret = read(pipefd[0], buf, sizeof(buf));
    EXPECT_EQ(0, ret);

    close(pipefd[0]);
}

TEST_F(BinderLibTest, PromoteLocal) {
    sp<IBinder> strong = new BBinder();
    wp<IBinder> weak = strong;
    sp<IBinder> strong_from_weak = weak.promote();
    EXPECT_TRUE(strong != NULL);
    EXPECT_EQ(strong, strong_from_weak);
    strong = NULL;
    strong_from_weak = NULL;
    strong_from_weak = weak.promote();
    EXPECT_TRUE(strong_from_weak == NULL);
}

TEST_F(BinderLibTest, PromoteRemote) {
    int ret;
    Parcel data, reply;
    sp<IBinder> strong = new BBinder();
    sp<IBinder> server = addServer();

    ASSERT_TRUE(server != NULL);
    ASSERT_TRUE(strong != NULL);

    ret = data.writeWeakBinder(strong);
    EXPECT_EQ(NO_ERROR, ret);

    ret = server->transact(BINDER_LIB_TEST_PROMOTE_WEAK_REF_TRANSACTION, data, &reply);
    EXPECT_GE(ret, 0);
}

class BinderLibTestService : public BBinder
{
    public:
        BinderLibTestService(int32_t id)
            : m_id(id)
            , m_nextServerId(id + 1)
            , m_serverStartRequested(false)
        {
            pthread_mutex_init(&m_serverWaitMutex, NULL);
            pthread_cond_init(&m_serverWaitCond, NULL);
        }
        ~BinderLibTestService()
        {
            exit(EXIT_SUCCESS);
        }
        virtual status_t onTransact(uint32_t code,
                                    const Parcel& data, Parcel* reply,
                                    uint32_t flags = 0) {
            //printf("%s: code %d\n", __func__, code);
            (void)flags;

            if (getuid() != (uid_t)IPCThreadState::self()->getCallingUid()) {
                return PERMISSION_DENIED;
            }
            switch (code) {
            case BINDER_LIB_TEST_REGISTER_SERVER: {
                int32_t id;
                sp<IBinder> binder;
                id = data.readInt32();
                binder = data.readStrongBinder();
                if (binder == NULL) {
                    return BAD_VALUE;
                }

                if (m_id != 0)
                    return INVALID_OPERATION;

                pthread_mutex_lock(&m_serverWaitMutex);
                if (m_serverStartRequested) {
                    m_serverStartRequested = false;
                    m_serverStarted = binder;
                    pthread_cond_signal(&m_serverWaitCond);
                }
                pthread_mutex_unlock(&m_serverWaitMutex);
                return NO_ERROR;
            }
            case BINDER_LIB_TEST_ADD_SERVER: {
                int ret;
                uint8_t buf[1] = { 0 };
                int serverid;

                if (m_id != 0) {
                    return INVALID_OPERATION;
                }
                pthread_mutex_lock(&m_serverWaitMutex);
                if (m_serverStartRequested) {
                    ret = -EBUSY;
                } else {
                    serverid = m_nextServerId++;
                    m_serverStartRequested = true;

                    pthread_mutex_unlock(&m_serverWaitMutex);
                    ret = start_server_process(serverid);
                    pthread_mutex_lock(&m_serverWaitMutex);
                }
                if (ret > 0) {
                    if (m_serverStartRequested) {
#if defined(HAVE_PTHREAD_COND_TIMEDWAIT_RELATIVE)
                        ret = pthread_cond_timeout_np(&m_serverWaitCond, &m_serverWaitMutex, 5000);
#else
                        struct timespec ts;
                        clock_gettime(CLOCK_REALTIME, &ts);
                        ts.tv_sec += 5;
                        ret = pthread_cond_timedwait(&m_serverWaitCond, &m_serverWaitMutex, &ts);
#endif
                    }
                    if (m_serverStartRequested) {
                        m_serverStartRequested = false;
                        ret = -ETIMEDOUT;
                    } else {
                        reply->writeStrongBinder(m_serverStarted);
                        reply->writeInt32(serverid);
                        m_serverStarted = NULL;
                        ret = NO_ERROR;
                    }
                } else if (ret >= 0) {
                    m_serverStartRequested = false;
                    ret = UNKNOWN_ERROR;
                }
                pthread_mutex_unlock(&m_serverWaitMutex);
                return ret;
            }
            case BINDER_LIB_TEST_NOP_TRANSACTION:
                return NO_ERROR;
            case BINDER_LIB_TEST_NOP_CALL_BACK: {
                Parcel data2, reply2;
                sp<IBinder> binder;
                binder = data.readStrongBinder();
                if (binder == NULL) {
                    return BAD_VALUE;
                }
                reply2.writeInt32(NO_ERROR);
                binder->transact(BINDER_LIB_TEST_CALL_BACK, data2, &reply2);
                return NO_ERROR;
            }
            case BINDER_LIB_TEST_GET_ID_TRANSACTION:
                reply->writeInt32(m_id);
                return NO_ERROR;
            case BINDER_LIB_TEST_INDIRECT_TRANSACTION: {
                int32_t count;
                uint32_t indirect_code;
                sp<IBinder> binder;

                count = data.readInt32();
                reply->writeInt32(m_id);
                reply->writeInt32(count);
                for (int i = 0; i < count; i++) {
                    binder = data.readStrongBinder();
                    if (binder == NULL) {
                        return BAD_VALUE;
                    }
                    indirect_code = data.readInt32();
                    BinderLibTestBundle data2(&data);
                    if (!data2.isValid()) {
                        return BAD_VALUE;
                    }
                    BinderLibTestBundle reply2;
                    binder->transact(indirect_code, data2, &reply2);
                    reply2.appendTo(reply);
                }
                return NO_ERROR;
            }
            case BINDER_LIB_TEST_SET_ERROR_TRANSACTION:
                reply->setError(data.readInt32());
                return NO_ERROR;
            case BINDER_LIB_TEST_GET_PTR_SIZE_TRANSACTION:
                reply->writeInt32(sizeof(void *));
                return NO_ERROR;
            case BINDER_LIB_TEST_GET_STATUS_TRANSACTION:
                return NO_ERROR;
            case BINDER_LIB_TEST_ADD_STRONG_REF_TRANSACTION:
                m_strongRef = data.readStrongBinder();
                return NO_ERROR;
            case BINDER_LIB_TEST_LINK_DEATH_TRANSACTION: {
                int ret;
                Parcel data2, reply2;
                sp<TestDeathRecipient> testDeathRecipient = new TestDeathRecipient();
                sp<IBinder> target;
                sp<IBinder> callback;

                target = data.readStrongBinder();
                if (target == NULL) {
                    return BAD_VALUE;
                }
                callback = data.readStrongBinder();
                if (callback == NULL) {
                    return BAD_VALUE;
                }
                ret = target->linkToDeath(testDeathRecipient);
                if (ret == NO_ERROR)
                    ret = testDeathRecipient->waitEvent(5);
                data2.writeInt32(ret);
                callback->transact(BINDER_LIB_TEST_CALL_BACK, data2, &reply2);
                return NO_ERROR;
            }
            case BINDER_LIB_TEST_WRITE_FILE_TRANSACTION: {
                int ret;
                int32_t size;
                const void *buf;
                int fd;

                fd = data.readFileDescriptor();
                if (fd < 0) {
                    return BAD_VALUE;
                }
                ret = data.readInt32(&size);
                if (ret != NO_ERROR) {
                    return ret;
                }
                buf = data.readInplace(size);
                if (buf == NULL) {
                    return BAD_VALUE;
                }
                ret = write(fd, buf, size);
                if (ret != size)
                    return UNKNOWN_ERROR;
                return NO_ERROR;
            }
            case BINDER_LIB_TEST_PROMOTE_WEAK_REF_TRANSACTION: {
                int ret;
                wp<IBinder> weak;
                sp<IBinder> strong;
                Parcel data2, reply2;
                sp<IServiceManager> sm = defaultServiceManager();
                sp<IBinder> server = sm->getService(binderLibTestServiceName);

                weak = data.readWeakBinder();
                if (weak == NULL) {
                    return BAD_VALUE;
                }
                strong = weak.promote();

                ret = server->transact(BINDER_LIB_TEST_NOP_TRANSACTION, data2, &reply2);
                if (ret != NO_ERROR)
                    exit(EXIT_FAILURE);

                if (strong == NULL) {
                    reply->setError(1);
                }
                return NO_ERROR;
            }
            case BINDER_LIB_TEST_DELAYED_EXIT_TRANSACTION:
                alarm(10);
                return NO_ERROR;
            case BINDER_LIB_TEST_EXIT_TRANSACTION:
                while (wait(NULL) != -1 || errno != ECHILD)
                    ;
                exit(EXIT_SUCCESS);
            default:
                return UNKNOWN_TRANSACTION;
            };
        }
    private:
        int32_t m_id;
        int32_t m_nextServerId;
        pthread_mutex_t m_serverWaitMutex;
        pthread_cond_t m_serverWaitCond;
        bool m_serverStartRequested;
        sp<IBinder> m_serverStarted;
        sp<IBinder> m_strongRef;
};

int run_server(int index, int readypipefd)
{
    status_t ret;
    sp<IServiceManager> sm = defaultServiceManager();
    {
        sp<BinderLibTestService> testService = new BinderLibTestService(index);
        if (index == 0) {
            ret = sm->addService(binderLibTestServiceName, testService);
        } else {
            sp<IBinder> server = sm->getService(binderLibTestServiceName);
            Parcel data, reply;
            data.writeInt32(index);
            data.writeStrongBinder(testService);

            ret = server->transact(BINDER_LIB_TEST_REGISTER_SERVER, data, &reply);
        }
    }
    write(readypipefd, &ret, sizeof(ret));
    close(readypipefd);
    //printf("%s: ret %d\n", __func__, ret);
    if (ret)
        return 1;
    //printf("%s: joinThreadPool\n", __func__);
    ProcessState::self()->startThreadPool();
    IPCThreadState::self()->joinThreadPool();
    //printf("%s: joinThreadPool returned\n", __func__);
    return 1; /* joinThreadPool should not return */
}

int main(int argc, char **argv) {
    int ret;

    if (argc == 3 && !strcmp(argv[1], "--servername")) {
        binderservername = argv[2];
    } else {
        binderservername = argv[0];
    }

    if (argc == 4 && !strcmp(argv[1], binderserverarg)) {
        return run_server(atoi(argv[2]), atoi(argv[3]));
    }

    ::testing::InitGoogleTest(&argc, argv);
    binder_env = AddGlobalTestEnvironment(new BinderLibTestEnv());
    ProcessState::self()->startThreadPool();
    return RUN_ALL_TESTS();
}

