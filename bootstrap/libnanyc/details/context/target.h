#pragma once
#include "libnanyc.h"
#include <yuni/core/smartptr/intrusive.h>
#include <yuni/string.h>
#include <nany/nany.h>
#include "source.h"
#include <unordered_map>
#include <unordered_set>

// forward declaration
namespace Yuni {
namespace Job {
class Taskgroup;
}
}




namespace ny {

class Project;
class BuildInfoContext;



class CTarget final
	: public Yuni::IIntrusiveSmartPtr<CTarget, false, Yuni::Policy::SingleThreaded> {
public:
	//! The class ancestor
	using Ancestor = Yuni::IIntrusiveSmartPtr<CTarget, false, Yuni::Policy::SingleThreaded>;
	//! The most suitable smart ptr for the class
	using Ptr = Ancestor::SmartPtrType<CTarget>::Ptr;
	//! Threading policy
	using ThreadingPolicy = Ancestor::ThreadingPolicy;


public:
	//! Get if a string is a valid target name
	static bool IsNameValid(const AnyString& name) noexcept;


public:
	//! \name Constructor & Destructor
	//@{
	//! Default constructor
	explicit CTarget(nyproject_t*, const AnyString& name);
	//! Copy constructor
	CTarget(nyproject_t*, const CTarget&);

	//! Destructor
	~CTarget();
	//@}

	//! Target name
	AnyString name() const;

	nytarget_t* self();
	const nytarget_t* self() const;

	void addSource(const AnyString& name, const AnyString& content);
	void addSourceFromFile(const AnyString& filename);


	void build(BuildInfoContext&, Yuni::Job::Taskgroup& task, Logs::Report& report);

	template<class T> void eachSource(const T& callback);


	//! \name Operators
	//@{
	//! Assignment
	CTarget& operator = (const CTarget&) = delete;
	//@}


private:
	//! Attached project
	nyproject_t* m_project = nullptr;
	//! Name of the target
	Yuni::ShortString64 m_name;
	//! All sources attached to the target
	std::vector<Source::Ptr> m_sources;
	//! All sources ordered by their filename
	std::unordered_map<AnyString, std::reference_wrapper<Source>> m_sourcesByFilename;
	//! All sources ordered by their name
	std::unordered_map<AnyString, std::reference_wrapper<Source>> m_sourcesByName;

	// friends
	friend class Context;

}; // class Target





} // namespace nany

#include "target.hxx"
