#pragma once

#include "Loot.h"

namespace Input
{
	class IHandler
	{
	public:
		virtual ~IHandler() = default;

		void operator()(RE::InputEvent* const& a_event) { DoHandle(a_event); }

	protected:
		virtual void DoHandle(RE::InputEvent* const& a_event) = 0;
	};

	class ScrollHandler :
		public IHandler
	{
	public:
		ScrollHandler();

	protected:
		void DoHandle(RE::InputEvent* const& a_event) override
		{
			using Device = RE::INPUT_DEVICE;

			for (auto iter = a_event; iter; iter = iter->next) {
				auto event = iter->AsIDEvent();
				if (!event || event->GetEventType() != RE::INPUT_EVENT_TYPE::kThumbstick) {
					continue;
				}
				auto thumbStickEvent = static_cast<RE::ThumbstickEvent*>(event);

				if (!thumbStickEvent->IsMainHand()) {
					continue;
				}

				if (!hasScrolled && std::abs(thumbStickEvent->yValue) > 0.9) {
					hasScrolled = true;
					if (thumbStickEvent->yValue > 0) {
						Loot::GetSingleton().ModSelectedIndex(-1.0);
					} else {
						Loot::GetSingleton().ModSelectedIndex(1.0);
					}

				} else if (hasScrolled && std::abs(thumbStickEvent->yValue) < 0.1) {
					hasScrolled = false;
				}
			}
		}

	private:
		bool hasScrolled = false;
	};

	class TakeHandler :
		public IHandler
	{
	protected:
		void DoHandle(RE::InputEvent* const& a_event) override
		{
			using VR = RE::BSOpenVRControllerDevice::Key;
			for (auto iter = a_event; iter; iter = iter->next) {
				const auto event = iter->AsButtonEvent();
				if (!event) {
					continue;
				}

				const auto controlMap = RE::ControlMap::GetSingleton();
				const auto idCode =
					controlMap ?
						controlMap->GetMappedKey("Activate", event->GetDevice()) :
						RE::ControlMap::kInvalid;

				if (event->GetIDCode() == idCode) {
					if (!_context && !event->IsDown()) {
						continue;
					}
					_context = true;
					if (event->IsHeld() && event->HeldDuration() > 1.0F) {
						auto& loot = Loot::GetSingleton();
						loot.Close();

						auto player = RE::PlayerCharacter::GetSingleton();
						if (player) {
							auto hand = player->isRightHandMainHand ?
								RE::VR_DEVICE::kRightController :
								RE::VR_DEVICE::kLeftController;
							auto task = SKSE::GetTaskInterface();
							task->AddTask([player, hand]() {
								player->ActivatePickRef(hand);
							});
						}

						_context = false;
						return;
					} else if (event->IsUp()) {
						TakeStack();
						_context = false;
						return;
					}
				}
			}
		}

	private:
		float GetGrabDelay() const
		{
			if (_grabDelay) {
				return _grabDelay->GetFloat();
			} else {
				assert(false);
				return std::numeric_limits<float>::max();
			}
		}

		void TakeStack();

		stl::observer<const RE::Setting*> _grabDelay{ RE::GetINISetting("fZKeyDelay:Controls") };
		bool _context{ false };
	};

	class TransferHandler :
		public IHandler
	{
	protected:
		void DoHandle(RE::InputEvent* const& a_event) override;
	};

	class Listeners :
		public RE::BSTEventSink<RE::InputEvent*>
	{
	public:
		Listeners()
		{
			_callbacks.push_back(std::make_unique<TakeHandler>());
			_callbacks.push_back(std::make_unique<ScrollHandler>());
			_callbacks.push_back(std::make_unique<TransferHandler>());
		}

		Listeners(const Listeners&) = default;
		Listeners(Listeners&&) = default;

		~Listeners() { Disable(); }

		Listeners& operator=(const Listeners&) = default;
		Listeners& operator=(Listeners&&) = default;

		void Enable()
		{
			auto input = RE::BSInputDeviceManager::GetSingleton();
			if (input) {
				input->AddEventSink(this);
			}
		}

		void Disable()
		{
			auto input = RE::BSInputDeviceManager::GetSingleton();
			if (input) {
				input->RemoveEventSink(this);
			}
		}

	private:
		using EventResult = RE::BSEventNotifyControl;

		EventResult ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*) override
		{
			const bool isShowing = Loot::GetSingleton().IsShowing();
			bool shouldStop = false;

			if (a_event) {
				for (auto event = *a_event; event; event = event->next) {
					const auto buttonEvent = event->AsButtonEvent();
					if (buttonEvent) {
						const auto controlMap = RE::ControlMap::GetSingleton();
						const auto idCode =
							controlMap ?
								controlMap->GetMappedKey("Activate", buttonEvent->GetDevice()) :
								RE::ControlMap::kInvalid;

						if (buttonEvent->GetIDCode() == idCode) {
							if (buttonEvent->IsDown()) {
								if (isShowing) {
									shouldStop = true;
									_isSuppressingRelease = true;
								}
							} else if (buttonEvent->IsUp()) {
								if (_isSuppressingRelease) {
									shouldStop = true;
									_isSuppressingRelease = false;
								}
							} else {
								if (_isSuppressingRelease) {
									shouldStop = true;
								}
							}
						}
					}
				}

				if (isShowing) {
					for (auto& callback : _callbacks) {
						(*callback)(*a_event);
					}
				}
			}

			return shouldStop ? EventResult::kStop : EventResult::kContinue;
		}

		std::vector<std::unique_ptr<IHandler>> _callbacks{};
		bool _isSuppressingRelease{ false };
	};
}
