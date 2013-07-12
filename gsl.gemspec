lib = File.expand_path('../lib/', __FILE__)
$:.unshift lib unless $:.include?(lib)

require 'nmatrix/version'

Gem::Specification.new do |gem|
  gem.name = "gsl"
  gem.version = File.readlines('VERSION')[0].chomp
  gem.summary = 'Ruby interface to GNU Scientific Library (NMatrix fork)'
  gem.description = 'Ruby/GSL is a Ruby interface to the GNU Scientific Library, for numerical computing with Ruby'
  gem.homepage = 'http://rb-gsl.rubyforge.org/'
  gem.authors = ['Yoshiki Tsunesada', 'David MacMahon', 'John Woods', 'Masaomi Hakateyama']
  gem.email = 'y-tsunesada@mm.em-net.ne.jp'

  gem.files         = `git ls-files`.split("\n")
  gem.test_files    = `git ls-files -- {test,spec,features}/*`.split("\n")
  #gem.executables   = `git ls-files -- bin/*`.split("\n").map{ |f| File.basename(f) }
  gem.extensions = %w[ ext/extconf.rb ]
  gem.require_paths = ["lib", "lib/gsl", "lib/ool", "ext"]

  gem.required_ruby_version = '>= 1.8.1'

  gem.add_dependency 'nmatrix', ">= 0.0.4"
  # gem.add_dependency 'narray', '>= 0.5.9'

  gem.has_rdoc = true
  gem.rdoc_options = [
      '--title', 'Ruby/GSL',
      '--main', 'rdoc/index.rdoc',
      '--exclude', 'ext/',
      '--exclude', 'include/',
      '--exclude', 'lib/',
    ]
  #gem.extra_rdoc_files = FileList['rdoc/*'].to_a


end

