// **********************************************************************
//
// Copyright (c) 2003
// ZeroC, Inc.
// Billerica, MA, USA
//
// All Rights Reserved.
//
// Ice is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License version 2 as published by
// the Free Software Foundation.
//
// **********************************************************************

#include <Ice/Ice.h>
#include <IceUtil/Thread.h>
#include <IceUtil/Time.h>
#include <TestCommon.h>
#include <Test.h>


using namespace std;
using namespace IceUtil;

class AMI_Servant_setValueAsyncI : public Test::AMI_Servant_setValueAsync
{
public:
    void ice_response() {}
    void ice_exception(const Ice::Exception&) {}
};

class ReadThread : public Thread
{
public:

    ReadThread(vector<Test::ServantPrx>& servants) :
	_servants(servants)
    {
    }
    
    virtual void
    run()
    {
	int loops = 10;
	while(loops-- > 0)
	{
	    try
	    {
		_servants[0]->getValue();
		test(false);
	    }
	    catch(const Ice::ObjectNotExistException&)
	    {
		// Expected
	    }
	    catch(...)
	    {
		test(false);
	    }
	    
	    for(int i = 1; i < static_cast<int>(_servants.size()); ++i)
	    {
		test(_servants[i]->getValue() == i);
	    }
	}
    }

private:
    vector<Test::ServantPrx>& _servants;
};

class ReadForeverThread : public Thread
{
public:

    ReadForeverThread(vector<Test::ServantPrx>& servants) :
	_servants(servants)
    {
    }
    
    virtual void
    run()
    {
	for(;;)
	{
	    try
	    {
		for(int i = 0; i < static_cast<int>(_servants.size()); ++i)
		{
		    test(_servants[i]->slowGetValue() == i);
		}
	    }
	    catch(const Ice::ConnectionRefusedException&)
	    {
		//
		// Expected
		//
		return;
	    }
	    catch(const Ice::LocalException& e)
	    {
		cerr << "Caught unexpected : " << e << endl;
		test(false);
		return;
	    }
	    catch(...)
	    {
		test(false);
		return;
	    }
	}
    }

private:
    vector<Test::ServantPrx>& _servants;
};

class AddForeverThread : public Thread
{
public:

    AddForeverThread(const Test::RemoteEvictorPrx& evictor, int id) :
	_evictor(evictor),
	_id(id)
    {
    }
    
    virtual void
    run()
    {
	int index = _id * 1000;

	for(;;)
	{
	    try
	    {
		_evictor->createServant(index++, 0);
	    }
	    catch(const Test::EvictorDeactivatedException&)
	    {
		//
		// Expected
		//
		return;
	    }
	    catch(const Ice::ObjectNotExistException&)
	    {
		//
		// Expected
		//
		return;
	    }
	    catch(const Ice::LocalException& e)
	    {
		cerr << "Caught unexpected : " << e << endl;
		test(false);
		return;
	    }
	    catch(...)
	    {
		test(false);
		return;
	    }
	}
    }

private:
    Test::RemoteEvictorPrx _evictor;
    int _id;
};



class CreateDestroyThread : public Thread
{
public:

    CreateDestroyThread(const Test::RemoteEvictorPrx& evictor, int id, int size) :
	_evictor(evictor),
	_id(id),
	_size(size)
    {
    }
    
    virtual void
    run()
    {
	try
	{
	    int loops = 50;
	    while(loops-- > 0)
	    {
		for(int i = 0; i < _size; i++)
		{
		    if(i == _id)
		    {
			//
			// Create when odd, destroy when even.
			//
			
			if(loops % 2 == 0)
			{
			    Test::ServantPrx servant = _evictor->getServant(i);
			    servant->destroy();
			    
			    //
			    // Twice
			    //
			    try
			    {
				servant->destroy();
				test(false);
			    }
			    catch(const Ice::ObjectNotExistException&)
			    {
				// Expected
			    }
			}
			else
			{
			    Test::ServantPrx servant = _evictor->createServant(i, i);
			    
			    //
			    // Twice
			    //
			    try
			    {
				servant = _evictor->createServant(i, 0);
				test(false);
			    }
			    catch(const Test::AlreadyRegisteredException&)
			    {
				// Expected
			    }
			}
		    }
		    else
		    {
			//
			// Just read/write the value
			//
			Test::ServantPrx servant = _evictor->getServant(i);
			try
			{
			    int val = servant->getValue();
			    test(val == i || val == -i);
			    servant->setValue(-val);
			}
			catch(const Ice::ObjectNotExistException&)
			{
			    // Expected from time to time
			}
		    }
		}
	    }
	}
	catch(...)
	{
	    //
	    // Unexpected!
	    //
	    test(false);
	}
    }
private:
    Test::RemoteEvictorPrx _evictor;
    int _id;
    int _size;
};



int
run(int argc, char* argv[], const Ice::CommunicatorPtr& communicator)
{
    string ref = "factory:default -p 12345 -t 30000";
    Ice::ObjectPrx base = communicator->stringToProxy(ref);
    test(base);
    Test::RemoteEvictorFactoryPrx factory = Test::RemoteEvictorFactoryPrx::checkedCast(base);

    cout << "testing Freeze Evictor... " << flush;
    
    const Ice::Int size = 5;
    Ice::Int i;
    
    Test::RemoteEvictorPrx evictor = factory->createEvictor("Test");
    
    evictor->setSize(size);
    
    //
    // Create some servants 
    //
    vector<Test::ServantPrx> servants;
    for(i = 0; i < size; i++)
    {
	servants.push_back(evictor->createServant(i, i));
	servants[i]->addFacet("facet1", "data");
	Test::FacetPrx facet1 = Test::FacetPrx::checkedCast(servants[i], "facet1");
	test(facet1);
	facet1->setValue(10 * i);
	facet1->addFacet("facet2", "moreData");
	Test::FacetPrx facet2 = Test::FacetPrx::checkedCast(facet1, "facet2");
	test(facet2);
	facet2->setValue(100 * i);
    }
   
    //
    // Evict and verify values.
    //
    evictor->setSize(0);
    evictor->setSize(size);
    for(i = 0; i < size; i++)
    {
	test(servants[i]->getValue() == i);
	Test::FacetPrx facet1 = Test::FacetPrx::checkedCast(servants[i], "facet1");
	test(facet1);
	test(facet1->getValue() == 10 * i);
	test(facet1->getData() == "data");
	Test::FacetPrx facet2 = Test::FacetPrx::checkedCast(facet1, "facet2");
	test(facet2);
	test(facet2->getData() == "moreData");
    }
    
    //
    // Mutate servants.
    //
    for(i = 0; i < size; i++)
    {
	servants[i]->setValue(i + 100);
	Test::FacetPrx facet1 = Test::FacetPrx::checkedCast(servants[i], "facet1");
	test(facet1);
	facet1->setValue(10 * i + 100);
	Test::FacetPrx facet2 = Test::FacetPrx::checkedCast(facet1, "facet2");
	test(facet2);
	facet2->setValue(100 * i + 100);
    }
    
    //
    // Evict and verify values.
    //
    evictor->setSize(0);
    evictor->setSize(size);
    for(i = 0; i < size; i++)
    {
	test(servants[i]->getValue() == i + 100);
	Test::FacetPrx facet1 = Test::FacetPrx::checkedCast(servants[i], "facet1");
	test(facet1);
	test(facet1->getValue() == 10 * i + 100);
	Test::FacetPrx facet2 = Test::FacetPrx::checkedCast(facet1, "facet2");
	test(facet2);
	test(facet2->getValue() == 100 * i + 100);
    }
    // 
    // Test saving while busy
    //
    Test::AMI_Servant_setValueAsyncPtr setCB = new AMI_Servant_setValueAsyncI;
    for(i = 0; i < size; i++)
    {
	//
	// Start a mutating operation so that the object is not idle.
	//
	servants[i]->setValueAsync_async(setCB, i + 300);
	//
	// Wait for setValueAsync to be dispatched.
	//
	ThreadControl::sleep(Time::milliSeconds(100));

	test(servants[i]->getValue() == i + 100);
	//
	// This operation modifies the object state but is not saved
	// because the setValueAsync operation is still pending.
	//
	servants[i]->setValue(i + 200);
	test(servants[i]->getValue() == i + 200);

	//
	// Force the response to setValueAsync
	//
	servants[i]->releaseAsync();
	test(servants[i]->getValue() == i + 300);
    }

    //
    // Add duplicate facet and catch corresponding exception
    // 
    for(i = 0; i < size; i++)
    {
	try
	{
	    servants[i]->addFacet("facet1", "foobar");
	    test(false);
	}
	catch(const Test::AlreadyRegisteredException&)
	{
	}
    }
    
    //
    // Remove a facet that does not exist
    // 
    try
    {
	servants[0]->removeFacet("facet3");
	test(false);
    }
    catch(const Test::NotRegisteredException&)
    {
    }

   
    //
    // Remove all facets
    //
    for(i = 0; i < size; i++)
    {
	servants[i]->removeFacet("facet1");
	servants[i]->removeFacet("facet2");
    }
    
    //
    // Check they are all gone
    //
    for(i = 0; i < size; i++)
    {
	Test::FacetPrx facet1 = Test::FacetPrx::checkedCast(servants[i], "facet1");
	test(facet1 == 0);
	Test::FacetPrx facet2 = Test::FacetPrx::checkedCast(servants[i], "facet2");
	test(facet2 == 0);
    }


    evictor->setSize(0);
    evictor->setSize(size);

   

    for(i = 0; i < size; i++)
    {
	test(servants[i]->getValue() == i + 300);

	Test::FacetPrx facet1 = Test::FacetPrx::checkedCast(servants[i], "facet1");
	test(facet1 == 0);
    }

    //
    // Destroy servants and verify ObjectNotExistException.
    //
    for(i = 0; i < size; i++)
    {
	servants[i]->destroy();
	try
	{
	    servants[i]->getValue();
	    test(false);
	}
	catch(const Ice::ObjectNotExistException&)
	{
	    // Expected
	}
    }
          
    //
    // Recreate servants, set transient value
    //  
    servants.clear();
    for(i = 0; i < size; i++)
    {
	servants.push_back(evictor->createServant(i, i));
	servants[i]->setTransientValue(i);
    }
    
    //
    // Create and destroy 100 servants to make sure we save and evict
    //
    for(i = 0; i < 100; i++)
    {
	Test::ServantPrx servant = evictor->createServant(size + i, size + i);
	servant->destroy();
    }
    
    //
    // Check the transient value
    //
    for(i = 0; i < size; i++)
    {
	test(servants[i]->getTransientValue() == -1);
    }
    
    //
    // Now with keep
    //
    for(i = 0; i < size; i++)
    {
	servants[i]->keepInCache();
	servants[i]->keepInCache();
	servants[i]->setTransientValue(i);
    }

    //
    // Create and destroy 100 servants to make sure we save and evict
    //
    for(i = 0; i < 100; i++)
    {
	Test::ServantPrx servant = evictor->createServant(size + i, size + i);
	servant->destroy();
    }
    
    //
    // Check the transient value
    //
    for(i = 0; i < size; i++)
    {
	test(servants[i]->getTransientValue() == i);
    }

    //
    // Again, after one release
    //
    for(i = 0; i < size; i++)
    {
	servants[i]->release();
    }
    for(i = 0; i < 100; i++)
    {
	Test::ServantPrx servant = evictor->createServant(size + i, size + i);
	servant->destroy();
    }
    for(i = 0; i < size; i++)
    {
	test(servants[i]->getTransientValue() == i);
    }

    //
    // Again, after a second release
    //
    for(i = 0; i < size; i++)
    {
	servants[i]->release();
    }
    for(i = 0; i < 100; i++)
    {
	Test::ServantPrx servant = evictor->createServant(size + i, size + i);
	servant->destroy();
    }
    for(i = 0; i < size; i++)
    {
	test(servants[i]->getTransientValue() == -1);
    }


    //
    // Release one more time
    //
    for(i = 0; i < size; i++)
    {
	try
	{
	    servants[i]->release();
	    test(false);
	}
	catch(const Test::NotRegisteredException&)
	{
	    // Expected
	}
    }


    // Deactivate and recreate evictor, to ensure that servants
    // are restored properly after database close and reopen.
    //
    evictor->deactivate();
    
    evictor = factory->createEvictor("Test");

    evictor->setSize(size);
    for(i = 0; i < size; i++)
    {
	servants[i] = evictor->getServant(i);
	test(servants[i]->getValue() == i);
    }

    //
    // Test concurrent lookups with a smaller evictor
    // size and one missing servant
    //
    evictor->setSize(size / 2);
    servants[0]->destroy();

    {
	const int threadCount = size * 2;
	
	ThreadPtr threads[threadCount];
	for(i = 0; i < threadCount; i++)
	{
	    threads[i] = new ReadThread(servants);
	    threads[i]->start();
	}
	
	for(i = 0; i < threadCount; i++)
	{
	    threads[i]->getThreadControl().join();
	}
    }
    
    //
    // Clean up.
    //
    evictor->destroyAllServants("");
    evictor->destroyAllServants("facet1");
    evictor->destroyAllServants("facet2");

    //
    // CreateDestroy threads
    //
    {
	const int threadCount = size;;
	
	ThreadPtr threads[threadCount];
	for(i = 0; i < threadCount; i++)
	{
	    threads[i] = new CreateDestroyThread(evictor, i, size);
	    threads[i]->start();
	}
	
	for(i = 0; i < threadCount; i++)
	{
	    threads[i]->getThreadControl().join();
	}

	//
	// Verify all destroyed
	// 
	for(i = 0; i < size; i++)   
	{
	    try
	    {
		servants[i]->getValue();
		test(false);
	    }
	    catch(const Ice::ObjectNotExistException&)
	    {
		// Expected
	    }
	}
    }

    //
    // Recreate servants.
    //  
    servants.clear();
    for(i = 0; i < size; i++)
    {
	servants.push_back(evictor->createServant(i, i));
    }

    //
    // Deactivate in the middle of remote AMD operations
    // (really testing Ice here)
    //
    {
	const int threadCount = size;
	
	ThreadPtr threads[threadCount];
	for(i = 0; i < threadCount; i++)
	{
	    threads[i] = new ReadForeverThread(servants);
	    threads[i]->start();
	}

	ThreadControl::sleep(Time::milliSeconds(500));
	evictor->deactivate();

	for(i = 0; i < threadCount; i++)
	{
	    threads[i]->getThreadControl().join();
	}
    }

    //
    // Resurrect
    //
    evictor = factory->createEvictor("Test");
    evictor->destroyAllServants("");

    //
    // Deactivate in the middle of adds
    //
    {
	const int threadCount = size;
	
	ThreadPtr threads[threadCount];
	for(i = 0; i < threadCount; i++)
	{
	    threads[i] = new AddForeverThread(evictor, i);
	    threads[i]->start();
	}

	ThreadControl::sleep(Time::milliSeconds(500));
	evictor->deactivate();
	
	for(i = 0; i < threadCount; i++)
	{
	    threads[i]->getThreadControl().join();
	}
    }
    
    
    //
    // Clean up.
    //
    evictor = factory->createEvictor("Test");
    evictor->destroyAllServants("");
    evictor->deactivate();

    cout << "ok" << endl;

    factory->shutdown();
    
    return EXIT_SUCCESS;
}

int
main(int argc, char* argv[])
{
    int status;
    Ice::CommunicatorPtr communicator;

    try
    {
        communicator = Ice::initialize(argc, argv);
        status = run(argc, argv, communicator);
    }
    catch(const Ice::Exception& ex)
    {
        cerr << ex << endl;
        status = EXIT_FAILURE;
    }

    if(communicator)
    {
        try
        {
            communicator->destroy();
        }
        catch(const Ice::Exception& ex)
        {
            cerr << ex << endl;
            status = EXIT_FAILURE;
        }
    }

    return status;
}
