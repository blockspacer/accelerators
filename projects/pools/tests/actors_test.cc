/**
  https://events.yandex.ru/lib/talks/2488/
  How make check(need thread)
  FIXME: how get result

  http://stackoverflow.com/questions/4548395/how-do-retrieve-the-thread-id-from-a-boostthread

  From Chromium:
    http://www.chromium.org/developers/lock-and-condition-variable
    These models are "threads + mutexes + condition-variables", and "threads + message-passing"

  Sean Parent
    http://channel9.msdn.com/events/GoingNative/2013/Cpp-Seasoning

  Trouble:
  - composition and dep. tasks - then()? ->> graph flow (TBB) etc.
  - "Callback hell"

  http://nickhutchinson.me/libdispatch/

  https://github.com/facebook/folly/tree/master/folly/futures
*/

#define BOOST_THREAD_PROVIDES_FUTURE

#include "pools/thread_pools.h"

#include <gtest/gtest.h>
#include <boost/bind/bind.hpp>
#include <boost/lexical_cast.hpp>

#include <stdio.h>

int no_safe_func()
{
  static int i;
  ++i;
  //try {
    //print("  task_int_1()\n");
    //throw 9;
    // http://www.boost.org/doc/libs/1_55_0/libs/exception/doc/frequently_asked_questions.html
    // https://groups.google.com/forum/#!topic/boost-list/E0C_gZDuydk
    BOOST_THROW_EXCEPTION(std::runtime_error(""));
    //boost::throw_exception(std::runtime_error(""));
    //error = boost::exception_ptr();
    return 1;
  //} catch (...) {
    //error = boost::current_exception();
  //}
}



// http://www.chromium.org/developers/design-documents/threading
class SingleWorker
{
public:
  // typedefs
  typedef boost::function0<void> Callable;

  // http://stackoverflow.com/questions/19192122/template-declaration-of-typedef-typename-footbar-bar
  //typedef
  //template <typename T>
  //boost::shared_ptr<boost::packaged_task<T> > Task;

  SingleWorker() : m_pool(1) { }

  void post(boost::function0<void> task) {
    m_pool.get().post(task);
  }

  // http://stackoverflow.com/questions/13157502/how-do-you-post-a-boost-packaged-task-to-an-io-service-in-c03
  //void post(packaged_task)  // no way, but...
  template<typename T>
  void post(boost::shared_ptr<boost::packaged_task<T> > task) {
    m_pool.get().post(boost::bind(
                        &boost::packaged_task<T>::operator (), task));
  }

  static std::string getCurrentThreadId() {
    return boost::lexical_cast<std::string>(boost::this_thread::get_id());
  }

  std::string getId() {
    boost::packaged_task<std::string> t(&getCurrentThreadId);
    boost::future<std::string> f = t.get_future();

    SingleWorker::Callable pkg
        = boost::bind(&boost::packaged_task<std::string>::operator(), boost::ref(t));
    post(pkg);

    return f.get();
  }

private:
  thread_pools::AsioThreadPool m_pool;
};

class Threads {
  static SingleWorker s_dbWorker;  // make weak access

public:
  static std::string dbId() {
    return s_dbWorker.getId();
  }

private:
  Threads();
};

SingleWorker Threads::s_dbWorker;


class NonThreadSafeObj
{
public:
  // Return future is hard. In task store ref to pack. task
  // may store shared_ptr

//private:
  void append() {
    m_s += "hello";
  }

private:
  std::string m_s;
};


TEST(AsPl, SingleThread)
{
  SingleWorker worker;

  boost::packaged_task<int> task(no_safe_func);
  boost::future<int> fi = task.get_future();

  SingleWorker::Callable f = boost::bind(&boost::packaged_task<int>::operator(), boost::ref(task));
  worker.post(f);

  EXPECT_THROW(fi.get(), std::runtime_error);
}

TEST(AsPl, SingleThreadShared)
{
  using boost::make_shared;
  using boost::packaged_task;
  using boost::shared_ptr;
  using boost::future;

  SingleWorker worker;
  SingleWorker worker1;

  NonThreadSafeObj obj;

  {
    shared_ptr<packaged_task<int> > t0 = make_shared<packaged_task<int> >(no_safe_func);
    future<int> f0 = t0->get_future();

    shared_ptr<packaged_task<int> > t1 = make_shared<packaged_task<int> >(no_safe_func);
    future<int> f1 = t1->get_future();

    worker.post(t0);
    worker.post(t1);  // worker1 - races

    EXPECT_THROW(f0.get(), std::runtime_error);
    EXPECT_THROW(f1.get(), std::runtime_error);

    std::cout << SingleWorker::getCurrentThreadId() << std::endl;
    std::cout << Threads::dbId() << std::endl;
    std::cout << worker.getId() << std::endl;
  }
}
