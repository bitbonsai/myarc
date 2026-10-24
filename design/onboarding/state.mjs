// Disposable storyboard state. Production workspace/import rules belong in Rust.
export const samples = ['standard', 'empty', 'large', 'failure']
export const lessons = ['Open a pin', 'Switch Space', 'Try the command bar']
const titles = ['Field notes', 'A quieter internet', 'Reading room', 'Weekend routes', 'Small discoveries', 'Studio journal']

export const makeSample = kind => {
  if (!samples.includes(kind)) throw new Error('Unknown study sample')
  const count = kind === 'empty' ? 0 : kind === 'large' ? 1000 : 18
  return {
    profiles: [{ id: 'personal', name: 'Personal' }, { id: 'work', name: 'Work' }],
    spaces: [
      { id: 'everyday', name: 'Everyday', profile: 'personal', color: '#bb603d' },
      { id: 'weekend', name: 'Weekend', profile: 'personal', color: '#457c72' },
      { id: 'studio', name: 'Studio', profile: 'work', color: '#526c9f' }
    ],
    pins: Array.from({ length: count }, (_, index) => ({
      id: `pin-${index}`,
      space: ['everyday', 'weekend', 'studio'][index % 3],
      title: `${titles[Math.floor(index / 3) % titles.length]}${index >= 18 ? ` ${index + 1}` : ''}`,
      url: `https://sample.example/notes/${index + 1}`
    }))
  }
}

export const initialState = () => ({
  scene: 'welcome', sample: 'standard', committed: false,
  activeSpace: 'everyday', selectedPin: null, lesson: 0, completed: [], error: null
})

export const transition = (state, event) => {
  switch (event.type) {
    case 'begin':
      return state.scene === 'welcome' ? { ...state, scene: 'source' } : state
    case 'sample':
      return state.scene === 'source' && samples.includes(event.sample)
        ? { ...state, sample: event.sample } : state
    case 'fresh':
      return state.scene === 'welcome' || state.scene === 'source'
        ? { ...initialState(), sample: 'empty', committed: true, scene: 'reveal' } : state
    case 'review':
      return state.scene === 'source' ? { ...state, scene: 'preview', error: null } : state
    case 'apply':
      return state.scene === 'preview' ? { ...state, scene: 'commit', error: null } : state
    case 'applied':
      return state.scene === 'commit' ? { ...state, committed: true, scene: 'reveal' } : state
    case 'failed':
      return state.scene === 'commit'
        ? { ...state, committed: false, scene: 'preview', error: 'Sample failure. Previous study workspace unchanged. Retry or choose another sample.' } : state
    case 'back':
      return ['source', 'preview'].includes(state.scene)
        ? { ...state, scene: state.scene === 'source' ? 'welcome' : 'source', error: null } : state
    case 'practice':
      return state.scene === 'reveal' && state.committed ? { ...state, scene: 'practice' } : state
    case 'pin': {
      if (!['practice', 'complete'].includes(state.scene)) return state
      const pin = makeSample(state.sample).pins.find(item => item.id === event.id && item.space === state.activeSpace)
      if (!pin) return state
      return { ...state, selectedPin: pin.id, completed: [...new Set([...state.completed, 0])] }
    }
    case 'space':
      if (!['practice', 'complete'].includes(state.scene) || !makeSample(state.sample).spaces.some(space => space.id === event.id)) return state
      return {
        ...state, activeSpace: event.id, selectedPin: null,
        completed: event.id === state.activeSpace ? state.completed : [...new Set([...state.completed, 1])]
      }
    case 'command':
      return ['practice', 'complete'].includes(state.scene)
        ? { ...state, completed: [...new Set([...state.completed, 2])] } : state
    case 'next':
      return state.scene === 'practice'
        ? { ...state, lesson: Math.min(2, state.lesson + 1), scene: state.lesson === 2 ? 'complete' : 'practice' } : state
    case 'skip':
      return ['reveal', 'practice'].includes(state.scene) && state.committed
        ? { ...state, scene: 'complete' } : state
    case 'replay':
      return initialState()
    default:
      return state
  }
}
