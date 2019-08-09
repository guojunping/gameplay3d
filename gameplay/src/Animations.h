#ifndef ANIMATIONS_H_
#define ANIMATIONS_H_

#include "Animation.h"

namespace gameplay
{

	/**
	* Animations contains all of the animations in the GamePlay
	*/
	class Animations  : public Ref
	{
	public:

		/**
		* Constructor.
		*/
		Animations(void);

		/**
		* Destructor.
		*/
		~Animations(void);

		void add(Animation* animation);
		unsigned int getAnimationCount() const;
		unsigned int getAnimations(std::vector<Animation*>& anmiations) const;
		Animation* findAnimation(const std::string & id) const;
		bool removeAnimation(Animation* animation);
		bool removeAnimation(const std::string & id);
	private:

		std::map<std::string, Animation*> _animations;
	};

}

#endif
