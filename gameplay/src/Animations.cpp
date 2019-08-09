#include "Base.h"
#include "Animations.h"

namespace gameplay
{

	Animations::Animations(void)
	{
	}

	Animations::~Animations(void)
	{
		for (; _animations.size() > 0;) {
			auto iter = _animations.begin();
			Animation* animation = iter->second;
			_animations.erase(iter);
			animation->release();
		}
	}

	void Animations::add(Animation* animation)
	{
		GP_ASSERT(animation);
		if (_animations.end() != _animations.find(animation->getId())) {
			GP_ERROR(" animation '%s' already exists !", animation->getId());
		}
		else {
			_animations.insert(std::make_pair(animation->getId(), animation));
			animation->addRef();
			animation->setAnimations(this);
		}
	}

	unsigned int Animations::getAnimationCount() const
	{
		return _animations.size();
	}

	unsigned int Animations::getAnimations(std::vector<Animation*>& anmiations) const 
	{
		for (auto iter : _animations) {
			anmiations.push_back(iter.second);
		}
		return _animations.size();
	}

	Animation* Animations::findAnimation(const std::string &id) const
	{
		auto iter = _animations.find(id);
		if (iter == _animations.end()) {
			return NULL;
		}
		return iter->second;
	}

	bool Animations::removeAnimation(Animation* animation) {
		auto iter = _animations.find(animation->getId());
		if (iter == _animations.end()) {
			return false;
		}
		_animations.erase(iter);
		animation->release();
		return true;
	}

	bool Animations::removeAnimation(const std::string &id) {
		auto iter = _animations.find(id);
		if (iter != _animations.end()) {
			Animation* animation = iter->second;
			_animations.erase(iter);
			animation->release();
			return true;
		}
		return false;
	}
}
