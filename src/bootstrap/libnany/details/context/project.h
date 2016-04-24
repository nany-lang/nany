#pragma once
#include "nany/nany.h"
#include <yuni/core/smartptr/intrusive.h>
#include <yuni/thread/mutex.h>
#include <map>
#include "target.h"



namespace Nany
{

	class Project final
		: public Yuni::IIntrusiveSmartPtr<Project, false, Yuni::Policy::SingleThreaded>
	{
	public:
		typedef Yuni::IIntrusiveSmartPtr<Project, false, Yuni::Policy::SingleThreaded> IntrusiveSmartPtr;
		typedef IntrusiveSmartPtr::Ptr Ptr;

	public:
		//! Ctor with an user-defined settings
		explicit Project(const nyproject_cf_t& cf);
		//! Copy ctor
		Project(const Project&) = delete;

		//! Initialize the project (after the ref count has been incremented)
		void init();
		//! Call the destructor and release this
		void destroy();


		nyproject_t* self();
		const nyproject_t* self() const;

		//! Allocate a new object
		template<class T, typename... Args> T* allocate(Args&&... args);
		//! delete an object
		template<class T> void deallocate(T* object);


		Project& operator = (const Project&) = delete;


	public:
		//! User settings
		nyproject_cf_t cf;

		struct {
			//! Default target
			CTarget::Ptr anonym;
			//! Default target
			CTarget::Ptr nsl;

			//! All other targets
			std::map<AnyString, CTarget::Ptr> all;
		}
		targets;

		//! Mutex
		mutable Yuni::Mutex mutex;


	private:
		//! Destructor, private, destroy() must be used instead
		~Project();

	}; // class Project




	//! Convert a nyproject_t into a Nany::Project
	Project& ref(nyproject_t* const);
	//! Convert a nyproject_t into a Nany::Project
	const Project& ref(const nyproject_t* const);


} // namespace Nany

#include "project.hxx"
