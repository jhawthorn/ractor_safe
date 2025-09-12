# frozen_string_literal: true

require "bundler/gem_tasks"
require "minitest/test_task"

Minitest::TestTask.create

require "rake/extensiontask"

task build: :compile

GEMSPEC = Gem::Specification.load("ractor_safe.gemspec")

Rake::ExtensionTask.new("ractor_safe", GEMSPEC) do |ext|
  ext.lib_dir = "lib/ractor_safe"
end

task default: %i[clobber compile test]
